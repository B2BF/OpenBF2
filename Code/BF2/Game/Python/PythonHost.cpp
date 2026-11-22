// Embedded Python host module, game-mode lifecycle, timers and triggers.
// Path attested by __FILE__ in initializeGameMode (ELF 0x815E9F0 and peers).
// Callback order, reference ownership and exception handling follow the shipped host.

#include "PyRef.h"
#include "py/PyArg.h"                     // py_arg<std::string> - log_write unpacks with it
#include "PythonComp.h"
#include "PythonHost.h"
#include "../../Core/Debug.h"
#include "../../Core/EventManager.h"        // registerEventHandler, in initialize()
#include "../../Core/SettingsRepostitory.h" // GSModDirectory, in initialize()
#include "../../Core/Ids.h"
#include "../../Core/System.h"          // g_system->getExactTime() - the timer clock
#include "../../IO/Console/Console.h"   // rcon_invoke -> mainConsole::executeLine
#include "../../Core/WeakPtr.h"
#include "../../Math/Vec3.h"
#include "../../Math/Mat4.h"
#include "../IGameLogic.h"              // the gameLogic singleton both per-frame gates read
#include "../IGame.h"                   // persistence wrappers query IGameServer
#include "../IGameServer.h"
#include "../GameLogic/ServerGameLogic.h" // pers_plrSetUnlocks
#include "../Persistence/PlayerUnlocks.h"
#include "../Persistence/PlayerStats.h"
#include "../Persistence/PlayerAwards.h"
#include "../EndOfRoundInfo.h"             // gl_sendEndOfRoundData data-block serializer
#include "../GameServer/GameServer.h"      // gl_sendEndOfRoundData broadcast
#include "../GameLogic/ScoreSystem/ScoreManager.h"
#include "../Persistence/IStatsManager.h" // GameSpy snapshot/new/end wrappers
#include "../Persistence/IPlayerStats.h"  // pers_getStatsKeyVal current-object view
#include "../../IO/File/FileManager.h"  // initializeGameMode probes the .py through io::fileManager
#include "../Objects/IObject.h"          // typed dispatch - the retired slot views
#include "../Objects/IObjectTemplate.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // trig_create's template lookup
#include "../Objects/ControlPointTemplate.h"   // updateTriggers' concrete CP template
#include "../Objects/ObjectLayoutAccess.h"      // the typed +0x14 template read
#include <list>
#include "../../Core/StringUtil.h"      // getLowerCase - the RCE guard's fold
#include <cstdio>                       // [TEMP] the unmasked callback-exception print
#include <cstdlib>                      // [TEMP] its getenv arm
#include "../../Scene/Object/PlayerManager.h"   // pmgr_getPlayers / pmgr_isIndexValid
#include "../../Scene/Object/PlayerScoreData.h" // pmgr_getScore / pmgr_setScore
#include <cstring>                      // strcmp - registerHandler's scriptEventMap walk
#include "../../Scene/Object/ObjectManager.h"      // trigger radius queries use the recovered class
#include "../../Scene/Object/ObjectPredicator.h"   // world::ObjectPredicator - the trigger sweep's base
#include "../Objects/IPlayer.h"          // the real interface - to_py(IPlayer*) reads getId
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h" // pmgr_p_get vehicle/weapon
#include "../Objects/ISoldier.h"          // pmgr_p_get kit query
#include "../Objects/Game/Kit.h"          // ISoldier::getKit -> IObject conversion
#include "../GameLogic/SquadManager/ISquadManager.h" // pmgr_getCommander

#include <algorithm>
#include <iterator>
#include <sstream>
#include <list>
#include <map>
#include <set>
#include <vector>

namespace dice {
namespace hfe {

extern IBase* squadManager;                  // ds:0x8ACBFA4 - Core/Globals.cpp
namespace world {
extern IBase* scoreManager;                  // ds:0x8AD1740 - Core/Globals.cpp
}

// Trigger and script callbacks use begin-round state 3 as the running-round gate.
const int BeginRoundState_Running = 3;

namespace world {
class IObject;


} // namespace world

namespace python {

// What the marshalling layer leans on. Neither has a header yet.
PyObject* wrapObject(world::IObject*);

// ds: ELF 0x8987204 / PE 0x82E3A4. Set only by pmgr_enableScoreEvents and read only by
// pmgr_setScore's total-score arm.
bool g_enableScoreEvents = false;


// Radius trigger with weak source tracking, Python callback/user data, and previous
// occupancy. Member names are [CHOSEN, not recovered]; the final eight bytes remain
// [UNRESOLVED]. Keep field ordering compatible with the trigger readers.
class RadiusTrigger
{
public:
    int                                  m_id;               // +0x00
    bool                                 m_onlyPCO;          // +0x04
    const world::IObjectTemplate*        m_objectTemplate;   // +0x08
    WeakPtr<world::IObject>              m_source;           // +0x0C
    float                                m_radius;           // +0x10
    // Owns callback references throughout trigger processing.
    PyRef                                m_callback;         // +0x14
    PyRef                                m_userData;         // +0x18
    std::set<WeakPtr<world::IObject> >   m_inside;           // +0x1C
    int                                  m_cullBehind;       // +0x28
    int                                  m_alwaysTrigger;    // +0x2C
};

// Trigger-grid predicates are constructed once per sweep; the template pointer
// is reassigned for each trigger.
class TemplatePredicator : public world::ObjectPredicator
{
public:
    TemplatePredicator() : m_objectTemplate(0) {}

    virtual bool         predicate(world::IObject* object);
    virtual unsigned int getFlags() const;
    virtual bool         performCulling(world::IObject* object);

    const world::IObjectTemplate* m_objectTemplate;                  // +0x04, from the trigger
};

class PCOPredicator : public world::ObjectPredicator
{
public:
    virtual bool         predicate(world::IObject* object);
    virtual unsigned int getFlags() const;
    virtual bool         performCulling(world::IObject* object);
};

// Python host implementation data. Preserve member order for reverse-order teardown;
// STL-containing member offsets vary by toolchain and are not a portable binary layout.
class PythonHostData
{
public:
    PythonHostData();
    ~PythonHostData();                                       // no symbol - inlined into ~PythonHost

    bool                                 m_initialized;      // +0x00  <- isInitialized() returns this
    // initialize(bool) caches its own argument here, and reinitialize()
    // reads it back to re-run initialize with the same setting. See the block over initialize.
    bool                                 m_byte01;           // +0x01  initialize's cached argument
    PyObject*                            m_bf2Module;        // +0x04
                                                             //   PyImport_ImportModule("bf2")
    PyObject*                            m_hostModule;       // +0x08  resolved: what
                                                             // Callable bf2.playerConvFunc used to wrap players; rejected during initialization if not callable.
    PyObject*                            m_playerConvFunc;   // +0x0C  bf2.playerConvFunc
    PyObject*                            m_objectClass;      // +0x10  the Python `Object` class
    // by reading PythonHost::handleEvent (0x8161DF0): this is the script-event
    // callback list. handleEvent walks it and calls every element with one int argument. What puts
    // entries in is still [UNRESOLVED] (initialize / initializeGameMode are unread).
    std::list<PyRef>                     m_list14;           // +0x14
    std::set<PyRef>                      m_timers;           // +0x18 (+0x1C count, +0x20 comparator)
    std::list<PyRef>                     m_timersToRemove;   // +0x24  removeTimer() push_back's here
    // Handlers indexed by ScriptEvent; each entry contains a callable and alwaysCall flag.
    // Registration sizes the table before dispatch.
    std::vector<std::vector<std::pair<PyRef, bool> > >
                                         m_vector28;         // +0x28 (+0x2C end, +0x30 capacity end)
    // Borrowed output target while getExceptionText captures Python traceback output.
    std::ostream*                        m_captureStream;    // +0x34
    std::list<RadiusTrigger>             m_triggers;         // +0x38
    std::list<RadiusTrigger>::iterator   m_triggerCursor;    // +0x3C  the round-robin cursor
    PyObject*                            m_adminScriptUpdate;// +0x40  called by update(), once a frame
    // Admin shutdown callback and an additional [UNRESOLVED] Python reference; both are decremented on destruction.
    PyObject*                            m_dword44;          // +0x44  admin-module shutdown callback
    // (was [UNRESOLVED]): initializeGameMode stores the game mode module's
    // "deinit" attribute here (0x815EFDE lookup -> PyRef assignment into +0x48). The mode contract
    // is init()/deinit(), and this is where deinit is bound - closing that [UNRESOLVED] too.
    PyObject*                            m_gameModeDeinit;   // +0x48
    bool                                 m_byte4C;           // +0x4C  suppresses timers AND triggers
};

// WAS a raw `+ 0x0C` read. Now that PythonHostData is declared (updateTriggers needs its
// containers as real types, not as offsets), the offset form would read the wrong bytes under MSVC's
// STL - the member is the only correct spelling. The layout note in PythonHost.h remains the record
// of where the field sits in the ELF.
inline PyObject* pyHostPlayerClass()
{
    return pyHostData()->m_playerConvFunc;                      // PythonHostData +0x0C
}

// wrapObject reads the Object class at ELF 8158237 / PE 5357DB..5357DE. Keep the
// accessor beside the complete owner: retail +0x10 becomes native x64 +0x20.
PyObject* pyHostObjectClass()
{
    return pyHostData()->m_objectClass;
}

// Releases the owned Python reference when present.
PyRef::~PyRef()
{
    Py_XDECREF(m_obj);
}

// Not self-assignment safe: releases the old reference before reading rhs.
// Preserve the original release-then-increment order and require non-aliasing assignment.
PyRef& PyRef::operator=(const PyRef& rhs)
{
    Py_XDECREF(m_obj);          // released first - see above
    m_obj = rhs.m_obj;
    Py_XINCREF(m_obj);
    return *this;
}

// C++-to-Python sequence conversions produce immutable tuples. PyTuple_SetItem
// steals each inserted reference; do not release it again after insertion.
PyObject* to_py(world::IObject* obj)
{
    return wrapObject(obj);
}

// . A 3-tuple of floats, walked as a float* rather than by member.
PyObject* to_py(const Vec3& v)
{
    PyObject* t = PyTuple_New(3);
    const float* p = &v.x;
    for (int i = 0; i < 3; ++i)
        PyTuple_SetItem(t, i, PyFloat_FromDouble(*p++));   // SetItem steals
    return t;
}

// . Size comes from the pointer difference (end - begin) >> 2.
PyObject* to_py(const std::vector<int>& v)
{
    PyObject* t = PyTuple_New((int)v.size());
    int i = 0;
    for (std::vector<int>::const_iterator it = v.begin(); it != v.end(); ++it)
        PyTuple_SetItem(t, i++, PyInt_FromLong(*it));
    return t;
}

// Constructs a Python Player from its integer id, not its C++ pointer. Returns a
// new reference to the wrapper or to None on every failure; never returns null.
PyObject* to_py(world::IPlayer* player)
{
    // ds:0x8990898 has no symbol - it is a copy relocation. The shape identifies it beyond doubt:
    // a static PyObject whose refcount is incremented and which is then returned, which is
    // Py_RETURN_NONE. [INFERRED] that it is _Py_NoneStruct; the behaviour is read.
    if (!player)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject* playerClass = pyHostPlayerClass();            // PythonHostData +0x0C
    if (!playerClass)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    PyObject* args = PyTuple_New(1);
    if (args)
    {
        PyTuple_SetItem(args, 0, PyInt_FromLong(player->getId()));   // IPlayer vtable +0x54
        PyObject* result = PyObject_CallObject(playerClass, args);
        if (result)
        {
            Py_DECREF(args);
            return result;                                   // the only non-None return
        }
    }

    Debug(DEBUGTYPE_6, "Game/Python/PythonHost.cpp", 970)
        << "failed to convert IPlayer* to a python Player object: "
        << pythonHost()->getExceptionText();

    Py_INCREF(Py_None);
    return Py_None;
}

// Converts the map to a tuple of (player, value) tuples, preserving iteration order.
PyObject* to_py(const std::map<world::IPlayer*, int>& m)
{
    PyObject* t = PyTuple_New((int)m.size());
    int i = 0;
    for (std::map<world::IPlayer*, int>::const_iterator it = m.begin(); it != m.end(); ++it)
    {
        PyObject* pair = PyTuple_New(2);
        PyTuple_SetItem(pair, 0, to_py(it->first));              // recurses into the Player path
        PyTuple_SetItem(pair, 1, PyInt_FromLong(it->second));
        PyTuple_SetItem(t, i++, pair);                           // steals `pair`
    }
    return t;
}

// Builds a tuple of tuples, copying each inner vector before conversion.
// Retain those per-element allocations.
PyObject* to_py(const std::vector<std::vector<int> >& v)
{
    PyObject* t = PyTuple_New((int)v.size());
    int i = 0;
    for (std::vector<std::vector<int> >::const_iterator it = v.begin(); it != v.end(); ++it)
    {
        std::vector<int> copy(*it);                              // the allocate + memmove pair
        PyTuple_SetItem(t, i++, to_py(copy));
    }
    return t;
}

// Builds a Python argument tuple. Before initialization every overload returns
// an empty PyRef without allocating or converting arguments.
PyRef buildArgs(const int& a0)
{
    if (!pythonHost()->isInitialized())
        return PyRef();                         // empty - see above

    PyObject* t = PyTuple_New(1);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));  // steals

    // The two-step is what the binary does and it is not redundant: PyRef's constructor INCREFs
    // (see PyRef.h), so the local's own reference has to go. Returning PyRef(t) without the decref
    // leaks one reference per call.
    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<int, int> - ELF 0x8105750, 178 B. Emitted beside uPlayingTicketSystem and used for
// ScriptEvent 26's `(oneBasedTeam, signedLimitId)` TicketLimitReached notification. It is the exact
// two-PyInt instance of the guarded tuple-builder pattern above.
PyRef buildArgs(const int& a0, const int& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, PyInt_FromLong(a1));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// Builds a one-item tuple for an initialized host. Player conversion uses
// PythonComp so repeated conversions retain the same Python wrapper identity.
PyRef buildArgs(world::IPlayer* const& a0)
{
    if (!pythonHost()->isInitialized())
        return PyRef();                         // empty - see the block above

    PyObject* t = PyTuple_New(1);
    PyTuple_SetItem(t, 0, to_py(a0));           // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<IPlayer*, int> - ELF 0x815BAB0 (178 B). pmgr_setScore uses it for ScriptEvent 28,
// passing the player object and the signed total-score delta.
PyRef buildArgs(world::IPlayer* const& a0, const int& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, to_py(a0));
    PyTuple_SetItem(t, 1, PyInt_FromLong(a1));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<IPlayer*, IPlayer*, IObject*, IObject*, IObject*> - 0x80B7A40, 301 B (emitted in
// GameServer's TU for handleNetworkEvent's arm 12, the client's kill-yourself request -> Python
// event 1). read AND the same guard, a 5-tuple of to_py(IPlayer*) x2 and
// to_py(IObject*) x3 (both overloads above, written), the same PyRef / decref two-step.
PyRef buildArgs(world::IPlayer* const& a0, world::IPlayer* const& a1, world::IObject* const& a2,
                world::IObject* const& a3, world::IObject* const& a4)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(5);
    PyTuple_SetItem(t, 0, to_py(a0));
    PyTuple_SetItem(t, 1, to_py(a1));
    PyTuple_SetItem(t, 2, to_py(a2));
    PyTuple_SetItem(t, 3, to_py(a3));
    PyTuple_SetItem(t, 4, to_py(a4));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<int, char const*> - 0x80B7B70, 178 B (the same TU, arm 18 -> Python event 38). read AND
// PyInt_FromLong + PyString_FromString into a 2-tuple. PyString_FromString
// on a null pointer is a CPython fault, and arm 18 passes the event's data pointer straight in.
PyRef buildArgs(const int& a0, const char* const& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, PyString_FromString(a1));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// Builds a two-argument tuple. Embedded NULs truncate the Python string.
PyRef buildArgs(const int& a0, const std::string& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, PyString_FromString(a1.c_str()));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<IPlayer*, int, int> - 0x80A4770, 227 B (emitted in Squad's TU; Squad::removeFromSquad
// fires Python event 11 with (player, squadId, 0)). read AND to_py(IPlayer*)
// then two PyInt_FromLong into a 3-tuple, the usual guard and two-step.
PyRef buildArgs(world::IPlayer* const& a0, const int& a1, const int& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, to_py(a0));
    PyTuple_SetItem(t, 1, PyInt_FromLong(a1));
    PyTuple_SetItem(t, 2, PyInt_FromLong(a2));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<int, IPlayer*, IPlayer*> - ELF 0x811FBD0 / PE 0x460FB0. SquadManager's
// leader-change event 13 carries the squad id, the old leader and the new leader. Both shipped
// ports use the usual initialization guard, then PyInt_FromLong followed by two player wrappers.
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, world::IPlayer* const& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, to_py(a1));
    PyTuple_SetItem(t, 2, to_py(a2));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<Team*, IPlayer*, IPlayer*> - ELF 0x811F700 / PE 0x460F10. CommanderManager's
// commander-change event 12 carries whether a Team pointer exists, then the old and new player
// wrappers. The first conversion is deliberately `team != 0`, not Team::getTeamId(): both shipped
// ports test the pointer and pass that boolean to PyInt_FromLong without dereferencing it.
PyRef buildArgs(Team* const& a0, world::IPlayer* const& a1, world::IPlayer* const& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0 != 0));
    PyTuple_SetItem(t, 1, to_py(a1));
    PyTuple_SetItem(t, 2, to_py(a2));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// buildArgs<int, std::string, std::string, int> - 0x80B7F80, 306 B (GameServer's TU;
// handleStringReceivedEvent fires Python event 39 with (senderId, message, channelName, flags)).
// read AND int, two c_str()-terminated strings, int, into a 4-tuple. The two
// `str[str.size()] = 0` stores in the ELF are c_str()'s terminator writes, as in the twin above.
PyRef buildArgs(const int& a0, const std::string& a1, const std::string& a2, const int& a3)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(4);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, PyString_FromString(a1.c_str()));
    PyTuple_SetItem(t, 2, PyString_FromString(a2.c_str()));
    PyTuple_SetItem(t, 3, PyInt_FromLong(a3));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// Builds an object-and-integer tuple, or an empty PyRef before host initialization.
PyRef buildArgs(world::IObject* const& a0, const int& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, to_py(a0));           // steals
    PyTuple_SetItem(t, 1, PyInt_FromLong(a1));  // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

//  (225 B), dedicated PE 0x5404D0: Python command arguments.
// Both images retain the initialized guard, three stealing tuple insertions in this order,
// and the PyRef/increment then decrement transfer. to_py(vector<int>) builds a nested tuple.
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, const std::vector<int>& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, to_py(a1));
    PyTuple_SetItem(t, 2, to_py(a2));

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

//  / PE 0x5D4D50: award response tuples.
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, const std::vector<std::vector<int>>& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();
    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, to_py(a1));
    PyTuple_SetItem(t, 2, to_py(a2));
    PyRef result(t);
    Py_DECREF(t);
    return result;
}

//  / PE 0x55FED0: five ordered validation arguments.
PyRef buildArgs(const std::string& a0, const std::string& a1, const int& a2,
                const int& a3, world::IPlayer* const& a4)
{
    if (!pythonHost()->isInitialized())
        return PyRef();
    PyObject* t = PyTuple_New(5);
    PyTuple_SetItem(t, 0, PyString_FromString(a0.c_str()));
    PyTuple_SetItem(t, 1, PyString_FromString(a1.c_str()));
    PyTuple_SetItem(t, 2, PyInt_FromLong(a2));
    PyTuple_SetItem(t, 3, PyInt_FromLong(a3));
    PyTuple_SetItem(t, 4, to_py(a4));
    PyRef result(t);
    Py_DECREF(t);
    return result;
}

//  / PE 0x5CFB90: persistence result and error responses.
PyRef buildArgs(const int& a0, world::IPlayer* const& a1, const std::string& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();
    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, PyInt_FromLong(a0));
    PyTuple_SetItem(t, 1, to_py(a1));
    PyTuple_SetItem(t, 2, PyString_FromString(a2.c_str()));
    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// Builds arguments for vehicle-entry callbacks. The three-argument form also
// reports whether the object's child soldier is active.
PyRef buildArgs(world::IPlayer* const& a0, world::IObject* const& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, to_py(a0));           // steals
    PyTuple_SetItem(t, 1, to_py(a1));           // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

PyRef buildArgs(world::IPlayer* const& a0, world::IObject* const& a1, const bool& a2)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(3);
    PyTuple_SetItem(t, 0, to_py(a0));                     // steals
    PyTuple_SetItem(t, 1, to_py(a1));                     // steals
    PyTuple_SetItem(t, 2, PyInt_FromLong(a2 ? 1 : 0));    // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// Builds a player-and-Boolean tuple for team-change callbacks. The Boolean is
// represented as a Python integer.
PyRef buildArgs(world::IPlayer* const& a0, const bool& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, to_py(a0));           // steals
    PyTuple_SetItem(t, 1, PyInt_FromLong(a1 ? 1 : 0));   // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

PyRef buildArgs(world::IObject* const& a0, world::IPlayer* const& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(2);
    PyTuple_SetItem(t, 0, to_py(a0));           // steals
    PyTuple_SetItem(t, 1, to_py(a1));           // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

PyRef buildArgs(world::IPlayer* const& a0, world::IPlayer* const& a1, world::IObject* const& a2,
                const std::map<world::IPlayer*, int>& a3, world::IObject* const& a4)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* t = PyTuple_New(5);
    PyTuple_SetItem(t, 0, to_py(a0));           // steals
    PyTuple_SetItem(t, 1, to_py(a1));           // steals
    PyTuple_SetItem(t, 2, to_py(a2));           // steals
    PyTuple_SetItem(t, 3, to_py(a3));           // steals - the tuple of (player, value) pairs
    PyTuple_SetItem(t, 4, to_py(a4));           // steals

    PyRef result(t);
    Py_DECREF(t);
    return result;
}

// PythonComp — the cache that gives Python object identity. Declaration and full reasoning are in
// PythonComp.h; these are the nine bodies, all of which live in this TU's address range.

PythonComp::PythonComp(PyObject* object)
    : m_refCount(1)                     // [eax+4] = 1
    , m_object(object)                  // [eax+8] = arg
{
}

void PythonComp::addRef()
{
    ++m_refCount;
}

int PythonComp::getRef()
{
    return m_refCount;
}

// Frees component storage without dropping m_object's Python reference; destroy must release that reference first.
int PythonComp::release()
{
    int remaining = --m_refCount;

    if (remaining == 0)
        delete this;

    return remaining;
}

// Use IID_IPythonComp to find the wrapper; getInterfaceID retains the base component ID.
void* PythonComp::queryInterface(unsigned int iid) const
{
    // The binary compares IID_IBase and IID_IObjectComponent with `jz` and IID_IPythonComp with
    // `cmovz`. Behaviourally identical - that is the shape GCC emits for a trailing `? :`.
    if (iid == IID_IBase || iid == world::IID_IObjectComponent)
        return const_cast<PythonComp*>(this);

    return (iid == IID_IPythonComp) ? const_cast<PythonComp*>(this) : 0;
}

// . The only place the wrapper's reference is dropped.
// IT does NOT NULL m_object afterwards. The binary decrefs [this+8] and stops; the field keeps
// pointing at what may now be freed memory. Preserved as written - a rebuild that nulls it would
// turn a use-after-free into a null dereference, which is a different failure, not the same one.
void PythonComp::destroy()
{
    Py_XDECREF(m_object);
}

//  — empty, argument ignored.
void PythonComp::setObject(world::IObject*)
{
}

//  — unconditionally null.
world::IObject* PythonComp::getObject() const
{
    return 0;
}

PyObject* PythonComp::getPythonObject() const
{
    return m_object;
}

// Static PythonHost owns its implementation data; the extra pointer gives free
// host-module functions access to that same state.
static PythonHostData* g_pyHostData = 0;

// ds:0x8AACDD4, immediately after it - a real object, not a pointer. Constructed at static-init.
static PythonHost g_pythonHost;

PythonHostData* pyHostData()
{
    return g_pyHostData;
}

PythonHost* pythonHost()
{
    return &g_pythonHost;
}

// Allocates and publishes host state without calling Python or registering events.
// initialize() starts the interpreter later. Other translation units must not access
// pyHostData() from static initialization because cross-unit construction order is unspecified.
PythonHost::PythonHost()
    : m_data(new PythonHostData())                           // operator new(0x50) at 0x815CD35
{
    // : `mov ds:8AACDD0h, ebx`. The same pointer, parked in the file-scope global so the
    // free `host` module functions can reach the state without a PythonHost* in hand. Not a cache
    // and not a second owner - m_data is the owner, this is a borrowed alias.
    g_pyHostData = m_data;
}

// Destroys host data without finalizing Python; shutdown must run first. The
// remaining object-class references still decref after interpreter shutdown,
// preserving the original teardown asymmetry.
PythonHost::~PythonHost()
{
    delete m_data;
}

// Initializes interpreter state and callback containers.
PythonHostData::PythonHostData()
    : m_initialized(false)                                   // +0x00
    , m_byte01(false)                                        // +0x01
    , m_bf2Module(0)                                           // +0x04
    , m_hostModule(0)                                           // +0x08
    , m_playerConvFunc(0)                                       // +0x0C
    , m_objectClass(0)                                       // +0x10
    , m_list14()                                             // +0x14, allocate(12)  0x8164008
    , m_timers()                                             // +0x18, allocate(20)  0x8164026
    , m_timersToRemove()                                     // +0x24, allocate(12)  0x8164058
    , m_vector28()                                           // +0x28..+0x30, three zeroed words
    , m_captureStream(0)                                     // +0x34
    , m_triggers()                                           // +0x38, allocate(56)  0x816408E
    // +0x3C IS zeroed, NOT SET TO end(). `mov dword ptr [ebx+3Ch], 0` at 0x816409E - the cursor
    // starts as a NULL iterator, not as a one-past-the-end iterator. updateTriggers' first pass is
    // what has to cope with that; see its note. Value-initialising it here reproduces the null.
    , m_triggerCursor()                                      // +0x3C
    , m_adminScriptUpdate(0)                                 // +0x40
    , m_dword44(0)                                           // +0x44
    , m_gameModeDeinit(0)                                      // +0x48
    , m_byte4C(false)                                        // +0x4C
{
}

// Releases Python references before automatic container destruction.
// [CHOSEN, not recovered] This groups decrefs ahead of container teardown rather
// than interleaving them; Python deallocators that re-enter can observe the difference.
PythonHostData::~PythonHostData()
{
    Py_XDECREF(m_gameModeDeinit);                            // +0x48
    Py_XDECREF(m_dword44);                                   // +0x44
    Py_XDECREF(m_adminScriptUpdate);                         // +0x40
    Py_XDECREF(m_objectClass);                               // +0x10
    Py_XDECREF(m_playerConvFunc);                               // +0x0C
    Py_XDECREF(m_hostModule);                                   // +0x08
    Py_XDECREF(m_bf2Module);                                   // +0x04

    // m_triggers, m_vector28, m_timersToRemove, m_timers and m_list14 are destroyed by the compiler
    // on the way out - which is what the ELF's inlined _List_base::__clear / _Rb_tree::_M_erase /
    // nested-vector walk are.
}

// Dispatches a script event when initialized and the dispatch gate is clear.
// Registration must size the handler table for eventId. Handlers run when marked
// alwaysCall or when the game is playing in begin-round state 3; Python call errors
// are cleared and successful results are released.
void PythonHost::handleEvent(ScriptEvent eventId, PyObject* args)
{
    if (!m_data->m_initialized)
        return;
    if (m_data->m_byte01)
        return;
    std::vector<std::pair<PyRef, bool> >& handlers = m_data->m_vector28[static_cast<int>(eventId)];
    for (std::vector<std::pair<PyRef, bool> >::iterator it = handlers.begin(); it != handlers.end(); ++it)
    {
        bool run = it->second;
        if (!run && gameLogic && gameLogic->getGameStatus() == GameStatus_Playing
            && static_cast<int>(gameLogic->getBeginRoundState()) == BeginRoundState_Running)
            run = true;
        if (!run)
            continue;
        PyObject* result = PyObject_CallObject(it->first.get(), args);
        if (result)
            Py_DECREF(result);
        else
            PyErr_Clear();
    }
}

// The `pythonHost.sendCommand` console method: fires script event 33 (ConsoleSendCommand) with
// (command, (args...)). The argument tuple ends at the last non-empty argument; empty ones before
// it stay as ''. No initialized check, unlike buildArgs. Always returns true.
bool PythonHost::consoleSendCommand(std::string command, std::string a1, std::string a2,
                                    std::string a3, std::string a4, std::string a5,
                                    std::string a6, std::string a7, std::string a8)
{
    const std::string* const args[8] = { &a1, &a2, &a3, &a4, &a5, &a6, &a7, &a8 };

    PyObject* const event = PyTuple_New(2);
    PyTuple_SetItem(event, 0, PyString_FromString(command.c_str()));        // steals

    int count = 0;
    for (int i = 7; i >= 0; --i)
    {
        if (!args[i]->empty())
        {
            count = i + 1;
            break;
        }
    }
    PyObject* const commandArgs = PyTuple_New(count);
    for (int i = 0; i < count; ++i)
        PyTuple_SetItem(commandArgs, i, PyString_FromString(args[i]->c_str()));
    PyTuple_SetItem(event, 1, commandArgs);

    handleEvent(static_cast<ScriptEvent>(33), event);
    Py_XDECREF(event);
    return true;
}

// Invalidates the Python player wrapper after native lookup removal but before
// deletion. Allocates arguments even before initialization and skips a missing
// converter. The original successful call result is deliberately not released.
void PythonHost::removePlayer(world::IPlayer* player)
{
    PyObject* args = PyTuple_New(1);
    if (!args)
        return;

    PyObject* const converter = pyHostPlayerClass();
    if (converter)
    {
        PyTuple_SetItem(args, 0, PyInt_FromLong(player->getId()));
        if (!PyObject_CallObject(converter, args))
        {
            Debug(DEBUGTYPE_6, "Game/Python/PythonHost.cpp", 951)
                << "failed to update removed player: " << pythonHost()->getExceptionText();
        }
    }

    Py_DECREF(args);
}

// Dispatches game-logic event 1 to Python while the global host is initialized and unmuted.
// Requires an integer user-data payload. Transitions into or out of code 4 update the
// timer/trigger suspension latch without notifying callbacks; repeated states notify them.
// [UNRESOLVED] The producer of the global mute byte m_byte01 is unknown.
void PythonHost::handleEvent(EventCategory category, unsigned int id, EventNode*, void* userData)
{
    // [UNRESOLVED] enumerator name for category 0; the value is read. Same for the id.
    if (!g_pyHostData->m_initialized)
        return;
    if (g_pyHostData->m_byte01)
        return;
    if (category != 0)
        return;
    if (id != 1)
        return;

    const int code = *static_cast<const int*>(userData);

    if (m_data->m_byte4C)
    {
        if (code != 4)
        {
            m_data->m_byte4C = false;
            return;
        }
    }
    else
    {
        if (code == 4)
        {
            m_data->m_byte4C = true;
            return;
        }
    }

    // The script-event callback sweep. end() IS RE-read every iteration (`mov eax, [ebx+4]; mov
    // eax, [eax+14h]` at 0x8161EDF, inside the loop) but the current node is not re-validated, so a
    // callback that erases its own entry leaves this walking a freed node. Not guarded in the
    // original; the re-read is reproduced because it is what lets a callback append safely.
    for (std::list<PyRef>::iterator it = m_data->m_list14.begin();
         it != m_data->m_list14.end(); ++it)
    {
        PyObject* result;
        {
            // buildArgs' own guard returns an empty PyRef when Python is down - see its note. The
            // binary passes that straight to PyObject_CallObject rather than testing it.
            PyRef args = buildArgs(code);
            result = PyObject_CallObject(it->get(), args);
        }   // args released here - the binary decrefs the tuple before it looks at the result

        // A raising callback IS swallowed, NOT logged. PyErr_Clear and nothing else - no Debug
        // line, no getExceptionText, and the callback stays registered. That is the opposite of
        // update()'s policy for the admin hook, and both are preserved as they are.
        if (!result)
        {
            // [TEMP] Reports otherwise-swallowed exceptions when the ghost tracer is enabled.
            // getExceptionText consumes the error; the following clear also covers the untraced path.
            // [RETIRE-WHEN: ghost-wire-verified]
            if (std::getenv("BF2_TRACE_GHOST"))
            {
                std::fprintf(stderr, "[PY] game-status callback raised:\n%s\n",
                             getExceptionText().c_str());
                std::fflush(stderr);
            }
            PyErr_Clear();
        }

        Py_XDECREF(result);
    }
}

// Python host-module entry points registered with METH_VARARGS. Their names and
// registration order form the script-facing interface.
PyObject* log_write(PyObject*, PyObject*);
PyObject* rcon_invoke(PyObject*, PyObject*);
PyObject* rcon_feedback(PyObject*, PyObject*);
PyObject* pmgr_getNumberOfPlayers(PyObject*, PyObject*);
PyObject* pmgr_getPlayers(PyObject*, PyObject*);
PyObject* pmgr_getCommander(PyObject*, PyObject*);
PyObject* pmgr_isIndexValid(PyObject*, PyObject*);
PyObject* pmgr_p_get(PyObject*, PyObject*);
PyObject* pmgr_p_set(PyObject*, PyObject*);
PyObject* pmgr_getScore(PyObject*, PyObject*);
PyObject* pmgr_setScore(PyObject*, PyObject*);
PyObject* pmgr_enableScoreEvents(PyObject*, PyObject*);
PyObject* timer_getWallTime(PyObject*, PyObject*);
PyObject* timer_getTimers(PyObject*, PyObject*);
PyObject* timer_created(PyObject*, PyObject*);
PyObject* timer_destroy(PyObject*, PyObject*);
PyObject* registerHandler(PyObject*, PyObject*);
PyObject* registerGameStatusHandler(PyObject*, PyObject*);
PyObject* unregisterGameStatusHandler(PyObject*, PyObject*);
PyObject* omgr_getObjectsOfType(PyObject*, PyObject*);
PyObject* omgr_getObjectsOfTemplate(PyObject*, PyObject*);
PyObject* sgl_getModDirectory(PyObject*, PyObject*);
PyObject* sgl_getMapName(PyObject*, PyObject*);
PyObject* sgl_getWorldSize(PyObject*, PyObject*);
PyObject* sgl_setParam(PyObject*, PyObject*);
PyObject* sgl_getParam(PyObject*, PyObject*);
PyObject* sgl_endGame(PyObject*, PyObject*);                        //   the round-end call
PyObject* sgl_getControlPointsInGroup(PyObject*, PyObject*);
PyObject* sgl_getIsAIGame(PyObject*, PyObject*);
PyObject* sgl_sendGameLogicEvent(PyObject*, PyObject*);
PyObject* sgl_sendPythonEvent(PyObject*, PyObject*);
PyObject* sgl_sendMedalEvent(PyObject*, PyObject*);
PyObject* sgl_sendRankEvent(PyObject*, PyObject*);
PyObject* sgl_sendHudEvent(PyObject*, PyObject*);
PyObject* sgl_sendTextMessage(PyObject*, PyObject*);
PyObject* gl_sendEndOfRoundData(PyObject*, PyObject*);
PyObject* ss_getParam(PyObject*, PyObject*);
PyObject* sgl_getSettingsBool(PyObject*, PyObject*);
PyObject* sh_setEnableCommander(PyObject*, PyObject*);
PyObject* pers_plrAwardMedal(PyObject*, PyObject*);
PyObject* pers_plrSetUnlocks(PyObject*, PyObject*);
PyObject* pers_plrRequestUnlocks(PyObject*, PyObject*);
PyObject* pers_plrRequestStats(PyObject*, PyObject*);
PyObject* pers_plrRequestAwards(PyObject*, PyObject*);
PyObject* pers_getStatsKeyVal(PyObject*, PyObject*);
PyObject* pers_gamespyStatsSendSnapshotUpdate(PyObject*, PyObject*);
PyObject* pers_gamespyStatsSendSnapshotFinal(PyObject*, PyObject*);
PyObject* pers_gamespyStatsEndGame(PyObject*, PyObject*);
PyObject* pers_gamespyStatsNewGame(PyObject*, PyObject*);
PyObject* trig_create(PyObject*, PyObject*);
PyObject* trig_getObjects(PyObject*, PyObject*);
PyObject* trig_destroy(PyObject*, PyObject*);
PyObject* trig_destroyAll(PyObject*, PyObject*);

// Trigger creation retains the source weak control block and resets the scan cursor.
// The optional seventh argument controls repeated triggering. None data is stored
// as null; <<PCO>> selects all player-control objects instead of a named template.
namespace {
// The ELF's anonymous-namespace counter (`v35 = ++trigger_id`). Ids start at 1, never reused.
int g_triggerId = 0;
} // namespace

PyObject* trig_create(PyObject*, PyObject* args)
{
    RadiusTrigger trigger;
    trigger.m_id             = ++g_triggerId;
    trigger.m_onlyPCO        = false;
    trigger.m_objectTemplate = 0;
    trigger.m_radius         = 0.0f;
    trigger.m_cullBehind     = 0;
    trigger.m_alwaysTrigger  = 0;

    ObjectToken* token         = 0;
    PyObject*    callback      = 0;
    std::string  templateName;
    double       radius        = 0.0;
    int          hemisphere    = 0;
    PyObject*    userData      = 0;
    int          alwaysTrigger = 0;

    const int failed = py_arg<ObjectToken*>(args, 0, token)
                     + py_arg<PyObject*>(args, 1, callback)
                     + py_arg<std::string>(args, 2, templateName)
                     + py_arg<double>(args, 3, radius)
                     + py_arg<int>(args, 4, hemisphere)
                     + py_arg<PyObject*>(args, 5, userData);
    if (failed || (PyTuple_Size(args) > 6 && py_arg<int>(args, 6, alwaysTrigger)))
        return 0;

    if (userData == Py_None)                     // None is dropped, not held
        userData = 0;

    WeakPtr<world::IObject>::Ctrl* const control =
        static_cast<WeakPtr<world::IObject>::Ctrl*>(tokenControlBlock(token));
    //  / PE 5322D1: the target member is +4 on x86, +8 on native x64.
    world::IObject* const source = control ? control->target : 0;
    if (!source)
    {
        PyErr_SetString(PyExc_RuntimeError, "trigger source object no longer exists");
        return 0;
    }

    if (!PyCallable_Check(callback))
    {
        PyErr_SetString(PyExc_RuntimeError, "expected a callable");
        return 0;
    }

    trigger.m_objectTemplate =
        world::objectTemplateManager->getTemplate(templateName);          // manager +0x24
    if (!trigger.m_objectTemplate)
    {
        if (templateName.compare("<<PCO>>") != 0)
        {
            PyErr_Format(PyExc_RuntimeError, "unknown object template '%s'", templateName.c_str());
            return 0;
        }
        trigger.m_onlyPCO = true;                // the magic name - any player-control object
    }

    // the retain-and-store the ELF spells out at 0x815BD96
    trigger.m_source.m_ctrl =
        reinterpret_cast<WeakPtr<world::IObject>::Ctrl*>(control);
    if (trigger.m_source.m_ctrl)
        ++trigger.m_source.m_ctrl->refCount;

    trigger.m_radius        = static_cast<float>(radius);
    trigger.m_callback      = callback;          // PyRef - increfs
    trigger.m_userData      = userData;          // PyRef - increfs (null stays null)
    trigger.m_cullBehind    = hemisphere;
    trigger.m_alwaysTrigger = alwaysTrigger;

    PythonHostData* const d = pyHostData();
    d->m_triggers.insert(d->m_triggers.end(), trigger);
    d->m_triggerCursor = d->m_triggers.begin();  // the invariant updateTriggers was missing

    return PyInt_FromLong(trigger.m_id);
}

// trig_getObjects / trig_destroy / trig_destroyAll - ELF 0x815C190 / 0x815C2A0 / 0x815C390,
//  / 0x532460 / 0x532520. read AND These stay beside the TU-local
// RadiusTrigger and PythonHostData until the attested py/Triggers.cpp boundary gains a header.
PyObject* trig_getObjects(PyObject*, PyObject* args)
{
    int triggerId = 0;
    if (py_arg<int>(args, 0, triggerId))
        return 0;

    PythonHostData* const data = pyHostData();
    std::list<RadiusTrigger>::iterator trigger = data->m_triggers.begin();
    while (trigger != data->m_triggers.end() && trigger->m_id != triggerId)
        ++trigger;
    if (trigger == data->m_triggers.end())
    {
        PyErr_SetString(PyExc_RuntimeError, "no such trigger");
        return 0;
    }

    PyObject* const result = PyTuple_New(static_cast<int>(trigger->m_inside.size()));
    if (!result)
        return 0;

    int index = 0;
    for (std::set<WeakPtr<world::IObject> >::const_iterator it = trigger->m_inside.begin();
         it != trigger->m_inside.end(); ++it)
    {
        world::IObject* object = it->m_ctrl ? it->m_ctrl->target : 0;
        PyTuple_SetItem(result, index++, wrapObject(object));
    }
    return result;
}

PyObject* trig_destroy(PyObject*, PyObject* args)
{
    int triggerId = 0;
    if (py_arg<int>(args, 0, triggerId))
        return 0;

    PythonHostData* const data = pyHostData();
    std::list<RadiusTrigger>::iterator trigger = data->m_triggers.begin();
    while (trigger != data->m_triggers.end() && trigger->m_id != triggerId)
        ++trigger;
    if (trigger == data->m_triggers.end())
    {
        PyErr_SetString(PyExc_RuntimeError, "no such trigger");
        return 0;
    }

    data->m_triggers.erase(trigger);
    data->m_triggerCursor = data->m_triggers.begin();
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* trig_destroyAll(PyObject*, PyObject*)
{
    PythonHostData* const data = pyHostData();
    data->m_triggers.clear();
    data->m_triggerCursor = data->m_triggers.begin();
    Py_INCREF(Py_None);
    return Py_None;
}

// Retains a callable game-status handler through PyRef. Muted script events ignore registration
// and still return a new reference to None. Invalid arguments return null with an exception;
// a non-callable argument raises RuntimeError.
PyObject* registerGameStatusHandler(PyObject*, PyObject* args)
{
    // the "script events are off" gate - see above. Silently ignored, still returns None.
    if (!pyHostData()->m_byte01)
    {
        PyObject* callback = 0;
        if (py_arg<PyObject*>(args, 0, callback))    // 1 means failed - see py/PyArg.h
            return 0;                                //   py_arg has already set the exception

        if (!PyCallable_Check(callback))
        {
            PyErr_SetString(PyExc_RuntimeError, "not callable");   // RuntimeError - see above
            return 0;
        }

        // the game-status handler list - see above. PyRef owns the reference from here on.
        pyHostData()->m_list14.push_back(PyRef(callback));
    }

    Py_INCREF(Py_None);                              // the ELF's `++_Py_NoneStruct`
    return Py_None;
}

// unregisterGameStatusHandler - ELF 0x8161D00 (232 B), PE 0x42B370 (178 B). It shares the
// registrar's m_byte01 mute gate and RuntimeError choice, and removes every duplicate occurrence.
PyObject* unregisterGameStatusHandler(PyObject*, PyObject* args)
{
    if (!pyHostData()->m_byte01)
    {
        PyObject* callback = 0;
        if (py_arg<PyObject*>(args, 0, callback))
            return 0;
        if (!PyCallable_Check(callback))
        {
            PyErr_SetString(PyExc_RuntimeError, "not callable");
            return 0;
        }

        std::list<PyRef>& handlers = pyHostData()->m_list14;
        std::list<PyRef>::iterator it = handlers.begin();
        while (it != handlers.end())
        {
            if (it->get() == callback)
                it = handlers.erase(it);
            else
                ++it;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}

// The small player-manager queries are complete in both shipped images and have no deeper frontier.
PyObject* pmgr_getNumberOfPlayers(PyObject*, PyObject*)
{
    return PyInt_FromLong(world::playerManager->getNumberOfPlayers());
}

PyObject* pmgr_getCommander(PyObject*, PyObject* args)
{
    int team = 0;
    if (py_arg<int>(args, 0, team))
        return 0;
    return PyInt_FromLong(static_cast<ISquadManager*>(squadManager)->getCommander(team));
}

// pmgr_p_get - ELF 0x815A0D0 (1497 B), PE 0x536620. The string key surface is the stock game
// mode's generic player binding. Object-valued keys return wrapped objects or an owned None; the
// weapon key alone consumes a third argument.
PyObject* pmgr_p_get(PyObject*, PyObject* args)
{
    std::string key;
    int playerId = 0;
    if (py_arg<std::string>(args, 0, key) + py_arg<int>(args, 1, playerId))
        return 0;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }

    if (key == "remote")      return PyInt_FromLong(player->getIsRemote() != 0);
    if (key == "ai")          return PyInt_FromLong(player->getIsAIPlayer() != 0);
    if (key == "alive")       return PyInt_FromLong(player->getIsAlive() != 0);
    if (key == "mandown")     return PyInt_FromLong(player->getIsManDown() != 0);
    if (key == "connected")   return PyInt_FromLong(player->getIsConnected() != 0);
    if (key == "fholder")     return PyInt_FromLong(player->getFlagHolder() != 0);
    if (key == "team")        return PyInt_FromLong(player->getTeam());
    if (key == "suicide")     return PyInt_FromLong(player->getSuicide() != 0);
    if (key == "ping")        return PyInt_FromLong(player->getPing());
    if (key == "tts")         return PyFloat_FromDouble(player->getTimeToSpawn());
    if (key == "sqid")        return PyInt_FromLong(player->getSquadId());
    if (key == "isql")        return PyInt_FromLong(player->getIsSquadLeader() != 0);
    if (key == "commander")   return PyInt_FromLong(player->getIsCommander() != 0);
    if (key == "name")        return PyString_FromString(player->getName().c_str());
    if (key == "sgr")         return PyInt_FromLong(player->getSpawnGroup());
    if (key == "profileid")   return PyInt_FromLong(player->getProfileId());

    world::IObject* object = 0;
    if (key == "kit")
    {
        world::IObject* const soldierObject = player->getDefaultVehicle();
        if (soldierObject && soldierObject->getTemplate()->getClassID() == world::CID_SoldierTemplate)
        {
            world::ISoldier* const soldier = static_cast<world::ISoldier*>(
                soldierObject->queryInterface(world::IID_ISoldier));
            object = static_cast<world::IObject*>(soldier->getKit());
        }
    }
    else if (key == "vehicle")
    {
        object = player->getVehicle();
        if (object)
        {
            world::IPlayerControlObject* const pco =
                static_cast<world::IPlayerControlObject*>(
                    object->queryInterface(world::IID_IPlayerControlObject));
            if (pco->isChildSoldierActive())
                object = player->getDefaultVehicle();
        }
    }
    else if (key == "defaultvehicle")
    {
        object = player->getDefaultVehicle();
    }
    else if (key == "weapon")
    {
        int index = 0;
        if (py_arg<int>(args, 2, index))
            return 0;
        world::IObject* const vehicle = player->getVehicle();
        if (vehicle)
        {
            world::IPlayerControlObject* const pco =
                static_cast<world::IPlayerControlObject*>(
                    vehicle->queryInterface(world::IID_IPlayerControlObject));
            object = pco->getActiveWeapon(index);
        }
    }
    else if (key == "addr")
    {
        return PyString_FromString(player->getPeerAddress().c_str());
    }
    else if (key == "isInsideCP")
    {
        return PyInt_FromLong(player->getIsInsideCP() != 0);
    }
    else
    {
        const std::string error = std::string("bad attribute: ") + key;
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return 0;
    }

    if (object)
        return wrapObject(object);
    Py_INCREF(Py_None);
    return Py_None;
}

// pmgr_p_set - ELF 0x815A6B0 (1214 B), PE 0x536C00. The team transform is deliberately odd:
// input 1 remains team 1 and every other value becomes team 2. Time-to-spawn seconds are converted
// with the shipped fixed 30 Hz simulation rate and added to the current WorldPref tick.
PyObject* pmgr_p_set(PyObject*, PyObject* args)
{
    std::string key;
    int playerId = 0;
    if (py_arg<std::string>(args, 0, key) + py_arg<int>(args, 1, playerId))
        return 0;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }

    if (key == "fholder")
    {
        int value = 0;
        if (py_arg<int>(args, 2, value)) return 0;
        player->setFlagHolder(value != 0);
    }
    else if (key == "team")
    {
        int value = 0;
        if (py_arg<int>(args, 2, value)) return 0;
        player->setTeam((value != 1) + 1, false);
    }
    else if (key == "suicide")
    {
        int value = 0;
        if (py_arg<int>(args, 2, value)) return 0;
        player->setSuicide(value != 0);
    }
    else if (key == "tts")
    {
        double seconds = 0.0;
        if (py_arg<double>(args, 2, seconds)) return 0;
        const int ticks = static_cast<int>(30.0 * seconds);
        player->setSpawnAtTick(static_cast<unsigned int>(WorldPref::getTickCount() + ticks));
    }
    else if (key == "name")
    {
        std::string value;
        if (py_arg<std::string>(args, 2, value)) return 0;
        player->setName(value);
    }
    else if (key == "sgr")
    {
        int value = 0;
        if (py_arg<int>(args, 2, value)) return 0;
        player->setSpawnGroup(value);
    }
    else if (key == "isInsideCP")
    {
        int value = 0;
        if (py_arg<int>(args, 2, value)) return 0;
        player->setIsInsideCP(value != 0);
    }
    else
    {
        const std::string error = std::string("attribute not implemented: ") + key;
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return 0;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

// buildBulletStatsTuple - ELF 0x8159F90 (310 B). Every map entry becomes
// (object-template-name, unsigned-short count); an unknown template contributes an empty name.
// Shipped quirk: failure to allocate the outer tuple returns an owned None with the allocation
// exception still pending instead of returning NULL.
PyObject* buildBulletStatsTuple(const std::map<unsigned int, unsigned short>& stats)
{
    PyObject* const result = PyTuple_New(static_cast<int>(stats.size()));
    if (!result)
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    int index = 0;
    for (std::map<unsigned int, unsigned short>::const_iterator it = stats.begin();
         it != stats.end(); ++it)
    {
        PyObject* const entry = PyTuple_New(2);
        std::string templateName;
        world::IObjectTemplate* const objectTemplate =
            world::objectTemplateManager->getTemplate(it->first);
        if (objectTemplate)
            templateName = objectTemplate->getName();

        PyTuple_SetItem(entry, 0, PyString_FromString(templateName.c_str()));
        PyTuple_SetItem(entry, 1, PyInt_FromLong(it->second));
        PyTuple_SetItem(result, index++, entry);
    }
    return result;
}

// pmgr_getScore - ELF 0x815AB50 (1476 B), PE 0x537360 (960 B). The string key set, map-clearing
// side effects and RuntimeError text are all cross-checked between the ports.
PyObject* pmgr_getScore(PyObject*, PyObject* args)
{
    int playerId = 0;
    std::string key;
    if (py_arg<int>(args, 0, playerId) + py_arg<std::string>(args, 1, key))
        return 0;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }
    world::PlayerScoreData* const score = player->getScoreData();

    if (key == "deaths")       return PyInt_FromLong(score->m_field10);
    if (key == "kills")        return PyInt_FromLong(score->m_field14);
    if (key == "TKs")          return PyInt_FromLong(score->m_field18);
    if (key == "score")        return PyInt_FromLong(score->m_score);
    if (key == "skillScore")   return PyInt_FromLong(score->m_field08);
    if (key == "rplScore")     return PyInt_FromLong(score->m_field04);
    if (key == "cmdScore")     return PyInt_FromLong(score->m_field0C);
    if (key == "fracScore")    return PyFloat_FromDouble(score->m_fractionalScore);
    if (key == "rank")         return PyInt_FromLong(score->m_rank);
    if (key == "firstPlace")   return PyInt_FromLong(score->m_field54);
    if (key == "secondPlace")  return PyInt_FromLong(score->m_field58);
    if (key == "thirdPlace")   return PyInt_FromLong(score->m_field5C);

    if (key == "bulletsFired")
        return buildBulletStatsTuple(score->m_bulletsFired);
    if (key == "bulletsGivingDamage")
        return buildBulletStatsTuple(score->m_bulletsGivingDamage);
    if (key == "bulletsFiredAndClear")
    {
        PyObject* const result = buildBulletStatsTuple(score->m_bulletsFired);
        score->m_bulletsFired.clear();
        return result;
    }
    if (key == "bulletsGivingDamageAndClear")
    {
        PyObject* const result = buildBulletStatsTuple(score->m_bulletsGivingDamage);
        score->m_bulletsGivingDamage.clear();
        return result;
    }

    const std::string error = std::string("no such score value: ") + key;
    PyErr_SetString(PyExc_RuntimeError, error.c_str());
    return 0;
}

// pmgr_setScore - ELF 0x815B120 (1401 B), PE 0x536E10 (855 B). All keys except score consume the
// integer scratch value; consequently a Python float supplied for one of those keys writes zero,
// an odd but shipped consequence of the shared parser. The score key preserves fractional carry.
PyObject* pmgr_setScore(PyObject*, PyObject* args)
{
    int playerId = 0;
    std::string key;
    if (py_arg<int>(args, 0, playerId) + py_arg<std::string>(args, 1, key))
        return 0;

    PyObject* const valueObject = PyTuple_GetItem(args, 2);
    unsigned int integerValue = 0;
    double doubleValue = 0.0;
    bool isDouble = false;
    if (PyInt_Check(valueObject))
    {
        if (py_arg<int>(args, 2, reinterpret_cast<int&>(integerValue)))
            return 0;
    }
    else
    {
        isDouble = true;
        if (py_arg<double>(args, 2, doubleValue))
            return 0;
    }

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }
    world::PlayerScoreData* const score = player->getScoreData();

    if (key == "deaths")
        score->m_field10 = static_cast<int>(integerValue);
    else if (key == "kills")
        score->m_field14 = static_cast<int>(integerValue);
    else if (key == "TKs")
        score->m_field18 = static_cast<int>(integerValue);
    else if (key == "score")
    {
        const int oldScore = score->m_score;
        if (isDouble)
        {
            const float narrowed = static_cast<float>(doubleValue);
            const int whole = static_cast<int>(narrowed);
            score->addFractionalScore(narrowed - static_cast<float>(whole));
            score->receiveFractionalScore();
            score->m_score += whole - oldScore;
        }
        else
        {
            score->m_score = static_cast<int>(integerValue);
        }

        if (g_enableScoreEvents)
        {
            const int delta = score->m_score - oldScore;
            PyRef eventArgs = buildArgs(player, delta);
            pythonHost()->handleEvent(static_cast<ScriptEvent>(28), eventArgs.get());
        }
    }
    else if (key == "skillScore")
        score->m_field08 = static_cast<int>(integerValue);
    else if (key == "rplScore")
        score->m_field04 = static_cast<int>(integerValue);
    else if (key == "cmdScore")
        score->m_field0C = static_cast<int>(integerValue);
    else if (key == "rank")
        score->setRank(integerValue, player);
    else
    {
        const std::string error = std::string("no such score value: ") + key;
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return 0;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* pmgr_enableScoreEvents(PyObject*, PyObject* args)
{
    int enabled = 0;
    if (py_arg<int>(args, 0, enabled))
        return 0;
    g_enableScoreEvents = enabled > 0;
    Py_INCREF(Py_None);
    return Py_None;
}

// pers_plrAwardMedal - ELF 0x81590F0 (190 B), PE 0x5328B0 (147 B). Despite its name, retail only
// parses four integers and validates the player id; the other three values have no side effect.
// Reproducing that surprising no-op is materially different from the former neutral stub because
// malformed arguments and unknown players now fail exactly as shipped.
PyObject* pers_plrAwardMedal(PyObject*, PyObject* args)
{
    int playerId = 0;
    int medal = 0;
    int level = 0;
    int when = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, medal)
                     + py_arg<int>(args, 2, level)
                     + py_arg<int>(args, 3, when);
    if (failed)
        return 0;

    (void)medal;
    (void)level;
    (void)when;
    if (!world::playerManager->getPlayerFromId(playerId))
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

// pers_plrSetUnlocks - ELF 0x81591B0 (924 B), PE 0x532ED0 (508 B). Two shipped defects are
// preserved: both source-list extractions use tuple slot 1 (slot 2 is ignored), and list integers
// are type-checked/read but the maps store zero rather than the values read. The stock script passes
// the same list twice, so the duplicate-slot defect is normally invisible.
PyObject* pers_plrSetUnlocks(PyObject*, PyObject* args)
{
    int playerId = 0;
    PyObject* sourceLists[2] = { 0, 0 };
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<PyObject*>(args, 1, sourceLists[0])
                     + py_arg<PyObject*>(args, 1, sourceLists[1]);      // slot 1 twice, shipped
    if (failed)
        return 0;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }

    std::map<int, int> unlocks[2];
    for (int team = 0; team != 2; ++team)
    {
        for (int index = 0; index < PyList_Size(sourceLists[team]); ++index)
        {
            PyObject* const item = PyList_GetItem(sourceLists[team], index);
            if (PyInt_Check(item))
            {
                PyInt_AsLong(item);          // parsed and deliberately discarded in both images
                PyErr_Occurred();            // result deliberately discarded
            }
            else
            {
                PyErr_SetString(PyExc_RuntimeError, "expected an integer");
            }
            unlocks[team][index] = 0;
        }
    }

    static_cast<ServerGameLogic*>(gameLogic)->setUnlocks(player, unlocks);
    Py_INCREF(Py_None);
    return Py_None;
}

// gl_sendEndOfRoundData - ELF 0x8154D10 (925 B), PE 0x534320 (486 B). The Python string is packed
// into EndOfRoundInfo data block 8, while the same call appends the round's map/outcome/team-ticket
// snapshot to GameLogic's history. The literal "$gamemode_fixme" is shipped behavior.
PyObject* gl_sendEndOfRoundData(PyObject*, PyObject* args)
{
    std::string data;
    if (py_arg<std::string>(args, 0, data))
        return 0;

    EndOfRoundInfo endOfRound;
    endOfRound.setData(data);
    endOfRound.setDelay(serverSettings->m_endOfRoundDelay);

    TicketHistory::Ticket ticket;
    ticket.string0 = game->getCurrentMapName();
    ticket.string4 = "$gamemode_fixme";

    world::ScoreManager* const scores = reinterpret_cast<world::ScoreManager*>(world::scoreManager);
    ticket.dwordC = scores->getVictoryType();
    ticket.dword10 = scores->getWinner();
    ticket.dword8 = game->getCurrentMapMaxPlayers();
    for (unsigned int team = 1; team <= 2; ++team)
    {
        ticket.pair[team - 1].text = gameLogic->getTeamName(static_cast<int>(team));
        ticket.pair[team - 1].value = scores->getTeamScore(team)->m_tickets;
    }

    gameLogic->getTicketHistory().tickets.push_back(ticket);

    const unsigned int size = endOfRound.getNetBufferSize();
    const void* const buffer = endOfRound.getNetBuffer();
    static_cast<GameServer*>(game)->sendDataBlockToAll(8, buffer, size, true);

    Py_INCREF(Py_None);
    return Py_None;
}

// pers_getStatsKeyVal - ELF 0x8159A60 (355 B), PE 0x532BD0 (227 B). This does not start a backend
// request: it reads map zero from the current persistence object that a completed request published
// on GameServer. The error text is part of the Python contract.
PyObject* pers_getStatsKeyVal(PyObject*, PyObject* args)
{
    std::string key;
    if (py_arg<std::string>(args, 0, key))
        return 0;

    IGameServer* gameServer = 0;
    if (game)
        gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
    IBase* const persistence = gameServer->getCurrentPersistenceObject();
    IPlayerStats* const stats = static_cast<IPlayerStats*>(
        persistence->queryInterface(IID_IPlayerStats));
    if (!stats)
    {
        PyErr_SetString(PyExc_RuntimeError,
                        "there is no current persistence object downloaded");
        return 0;
    }

    const std::string value = stats->getKeyValue(0, key);
    return PyString_FromString(value.c_str());
}

// The GameSpy stats quartet - ELF 0x8159BB0 / 0x8159C50 / 0x8159CF0 / 0x8159D40, PE 0x532CB0 /
//  / 0x532830 / 0x532870. Their full IGame -> IGameServer -> IStatsManager chain is typed.
// The two string wrappers deliberately ignore py_arg's status and still call the manager, matching
// retail's unusual "perform side effect, then return None with a pending Python error" behavior.
static IStatsManager* hostStatsManager()
{
    IGameServer* gameServer = 0;
    if (game)
        gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
    return static_cast<IStatsManager*>(gameServer->getStatsManager());
}

PyObject* pers_gamespyStatsSendSnapshotUpdate(PyObject*, PyObject* args)
{
    std::string snapshot;
    py_arg<std::string>(args, 0, snapshot);          // return deliberately ignored
    hostStatsManager()->sendSnapshot(snapshot);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* pers_gamespyStatsSendSnapshotFinal(PyObject*, PyObject* args)
{
    IStatsManager* const manager = hostStatsManager();
    std::string snapshot;
    py_arg<std::string>(args, 0, snapshot);          // return deliberately ignored
    manager->sendFinal(snapshot);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* pers_gamespyStatsEndGame(PyObject*, PyObject*)
{
    hostStatsManager()->endGame();
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* pers_gamespyStatsNewGame(PyObject*, PyObject*)
{
    hostStatsManager()->newGame();
    Py_INCREF(Py_None);
    return Py_None;
}

// timer_getWallTime / timer_created / timer_destroy - ELF 0x8161740 / 0x8161760 / 0x81617B0,
//  / 0x42B1D0 / 0x42B260. The timer object itself is opaque to these wrappers.
PyObject* timer_getWallTime(PyObject*, PyObject*)
{
    return PyFloat_FromDouble(g_system->getExactTime());
}

// timer_getTimers - ELF 0x8161800 (144 B), PE 0x42B0D0. The shipped loop never advances its
// destination index: every timer is installed into tuple slot zero, leaking the earlier slot-zero
// references and leaving the other tuple entries null. This dangerous retail defect is preserved;
// an empty timer set, which is the ordinary boot case, still returns a valid empty tuple.
PyObject* timer_getTimers(PyObject*, PyObject*)
{
    PythonHostData* const data = pyHostData();
    PyObject* const result = PyTuple_New(static_cast<int>(data->m_timers.size()));
    if (!result)
        return 0;

    for (std::set<PyRef>::const_iterator it = data->m_timers.begin();
         it != data->m_timers.end(); ++it)
    {
        PyObject* const timer = it->get();
        Py_INCREF(timer);
        PyTuple_SET_ITEM(result, 0, timer);               // shipped: index never increments
    }
    return result;
}

PyObject* timer_created(PyObject*, PyObject* args)
{
    PyObject* timer = 0;
    if (py_arg<PyObject*>(args, 0, timer))
        return 0;
    pythonHost()->insertTimer(timer);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* timer_destroy(PyObject*, PyObject* args)
{
    PyObject* timer = 0;
    if (py_arg<PyObject*>(args, 0, timer))
        return 0;
    pythonHost()->removeTimer(timer);
    Py_INCREF(Py_None);
    return Py_None;
}

// rcon_invoke - ELF 0x8162110, PE 0x429BA0. Non-empty commands and results are newline-terminated
// before/after the five-false executeLine call. An empty command bypasses the console entirely.
PyObject* rcon_invoke(PyObject*, PyObject* args)
{
    std::string command;
    if (py_arg<std::string>(args, 0, command))
        return 0;
    if (command.empty())
        return PyString_FromString("");

    if (command[command.size() - 1] != '\n')
        command.append("\n");

    std::string output;
    io::mainConsole->executeLine(command, output, false, false, false, false, false);
    if (output.empty() || output[output.size() - 1] != '\n')
        output.append("\n");
    return PyString_FromString(output.c_str());
}

// rcon_feedback - ELF 0x8162310, PE 0x429CB0. ECNetwork 19 consumes the local synchronously as
// { connection id, text }; the "rcon: " prefix is part of the shipped payload, not presentation in
// the receiver.
PyObject* rcon_feedback(PyObject*, PyObject* args)
{
    int connectionId = 0;
    std::string input;
    if (py_arg<int>(args, 0, connectionId) + py_arg<std::string>(args, 1, input))
        return 0;

    struct SendDataBlockRequest
    {
        int connectionId;
        std::string text;
    } request;
    request.connectionId = connectionId;
    request.text = std::string("rcon: ") + input;
    eventManager->postEvent(ECNetwork, 19, &request, 0.0f);
    Py_INCREF(Py_None);
    return Py_None;
}

// Request player unlocks. GameServer owns the adjusted HTTPRequest base before
// starting the request because completion may run synchronously inside getUnlocks.
PyObject* pers_plrRequestUnlocks(PyObject*, PyObject* args)
{
    int playerId, asynchronous;
    if (py_arg<int>(args, 0, playerId) + py_arg<int>(args, 1, asynchronous))
        return 0;
    world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
    {
        PyErr_SetString(PyExc_RuntimeError, "no such player");
        return 0;
    }
    std::vector<int> unlocks;
    PlayerUnlocks* request = new PlayerUnlocks;
    IGameServer* server = game
        ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    if (server)
        server->addPersistenceConnection(static_cast<HTTPRequest*>(request));
    return PyInt_FromLong(request->getUnlocks(player->getProfileId(), asynchronous != 0, unlocks));
}
//  / PE 0x532950. Values above 255 are profile ids, otherwise player ids.
PyObject* pers_plrRequestStats(PyObject*, PyObject* args)
{
    int playerId, asynchronous;
    std::string query;
    if (py_arg<int>(args, 0, playerId) + py_arg<int>(args, 1, asynchronous)
        + py_arg<std::string>(args, 2, query))
        return 0;
    int profileId = playerId;
    if (playerId <= 255)
    {
        world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
        if (!player)
        {
            PyErr_SetString(PyExc_RuntimeError, "no such player");
            return 0;
        }
        profileId = player->getProfileId();
    }
    PlayerStats* request = new PlayerStats;
    IGameServer* server = game
        ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    if (server)
        server->addPersistenceConnection(static_cast<HTTPRequest*>(request));
    return PyInt_FromLong(request->getStats(profileId, query, asynchronous != 0, true));
}
//  / PE 0x532A90. The third string argument is required but unused.
PyObject* pers_plrRequestAwards(PyObject*, PyObject* args)
{
    int playerId, asynchronous;
    std::string query;
    if (py_arg<int>(args, 0, playerId) + py_arg<int>(args, 1, asynchronous)
        + py_arg<std::string>(args, 2, query))
        return 0;
    int profileId = playerId;
    if (playerId <= 255)
    {
        world::IPlayer* player = world::playerManager->getPlayerFromId(playerId);
        if (!player)
        {
            PyErr_SetString(PyExc_RuntimeError, "no such player");
            return 0;
        }
        profileId = player->getProfileId();
    }
    PlayerAwards* request = new PlayerAwards;
    IGameServer* server = game
        ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    if (server)
        server->addPersistenceConnection(static_cast<HTTPRequest*>(request));
    return PyInt_FromLong(request->getAwards(profileId, 0, asynchronous != 0, true));
}
// Null-terminated script-event name mapping used by registerHandler. Names and
// values define the script interface; preserve the table's explicit mapping.
struct ScriptEventMapEntry { const char* name; int event; };
const ScriptEventMapEntry scriptEventMap[] =
{
    { "Reset",                       0 },  { "PlayerKilled",               1 },
    { "PlayerRevived",               2 },  { "PlayerDeath",                3 },
    { "PickupKit",                   4 },  { "DropKit",                    5 },
    { "PlayerChangeWeapon",          6 },  { "PlayerChangeTeams",          7 },
    { "DeployGrapplingHook",         8 },  { "DeployZipLine",              9 },
    { "DeployTactical",             10 },  { "PlayerChangedSquad",        11 },
    { "ChangedCommander",           12 },  { "ChangedSquadLeader",        13 },
    { "PlayerConnect",              14 },  { "PlayerDisconnect",          15 },
    { "PlayerBanned",               16 },  { "PlayerKicked",              17 },
    { "PlayerUnlocksResponse",      18 },  { "PlayerStatsResponse",       19 },
    { "PlayerAwardsResponse",       20 },  { "EnterVehicle",              21 },
    { "ExitVehicle",                22 },  { "PlayerSpawn",               23 },
    { "VehicleDestroyed",           24 },  { "ControlPointChangedOwner",  25 },
    { "TicketLimitReached",         26 },  { "TimeLimitReached",          27 },
    { "PlayerScore",                28 },  { "PlayerRepairPoint",         29 },
    { "PlayerHealPoint",            30 },  { "PlayerGiveAmmoPoint",       31 },
    { "PlayerTeamDamagePoint",      32 },  { "ConsoleSendCommand",        33 },
    { "ClientCommand",              34 },  { "ServerReady",               35 },
    { "BeginRound",                 36 },  { "EndRound",                  37 },
    { "RemoteCommand",              38 },  { "ChatMessage",               39 },
    { "ValidatePlayerNameResponse", 40 },  { "SpottedMessageSent",        41 },
    { 0,                             0 },
};

// Registers a callable for a named script event, optionally bypassing normal round
// gating. Minimal mode ignores requests before parsing. Rejects noncallables/unknown
// names; duplicate callable pointers for the same event are ignored.
PyObject* registerHandler(PyObject*, PyObject* args)
{
    if (pyHostData()->m_byte01)                          // the minimal-mode mute, first
    {
        Py_INCREF(Py_None);
        return Py_None;
    }

    std::string name;
    PyObject* callable = 0;
    int alwaysCall = 0;

    const int failed = py_arg<std::string>(args, 0, name)
                     + py_arg<PyObject*>(args, 1, callable);
    if (failed)
        return 0;
    if (PyTuple_Size(args) > 2 && py_arg<int>(args, 2, alwaysCall))
        return 0;

    if (!PyCallable_Check(callable))
    {
        PyErr_SetString(PyExc_RuntimeError, "not callable");
        return 0;
    }

    const ScriptEventMapEntry* entry = scriptEventMap;
    for (; entry->name; ++entry)
    {
        if (std::strcmp(entry->name, name.c_str()) == 0)
            break;
    }
    if (!entry->name)
    {
        PyErr_SetString(PyExc_RuntimeError, "illegal event name");
        return 0;
    }

    std::vector<std::pair<PyRef, bool> >& slot = pyHostData()->m_vector28[entry->event];
    for (std::vector<std::pair<PyRef, bool> >::const_iterator it = slot.begin();
         it != slot.end(); ++it)
    {
        if (it->first.get() == callable)                 // dedup by raw pointer
        {
            Py_INCREF(Py_None);
            return Py_None;
        }
    }
    slot.push_back(std::make_pair(PyRef(callable), alwaysCall > 0));

    Py_INCREF(Py_None);
    return Py_None;
}

// pmgr_getPlayers - ELF 0x8159E40, read in full. Returns a tuple OF player
// IDS (ints), not player objects - the bf2-side python wraps ids through playerConvFunc itself.
// A failed PyTuple_New returns 0 with CPython's own MemoryError standing.
// (The 0x8159xxx address is the pmgr TU's; body here on the established central-python precedent.)
PyObject* pmgr_getPlayers(PyObject*, PyObject*)
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();   // +0x3C

    int count = 0;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
        ++count;

    PyObject* const tuple = PyTuple_New(count);
    if (!tuple)
        return 0;

    int index = 0;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it, ++index)
    {
        PyTuple_SetItem(tuple, index, PyInt_FromLong((*it)->getId()));   // vt +0x54; steals
    }
    return tuple;
}

// pmgr_isIndexValid - ELF 0x8159F30, read in full. PyInt of "does
// getPlayerFromId answer non-null".
PyObject* pmgr_isIndexValid(PyObject*, PyObject* args)
{
    int id = 0;
    if (py_arg<int>(args, 0, id))
        return 0;
    return PyInt_FromLong(world::playerManager->getPlayerFromId(id) != 0);
}


// ds:0x8987400. THE order IS THE binary'S, not alphabetical and not grouped - reproduced exactly.
PyMethodDef method_table[] =
{
    { "log",                         log_write,                          METH_VARARGS, 0 },
    { "rcon_invoke",                 rcon_invoke,                        METH_VARARGS, 0 },
    { "rcon_feedback",               rcon_feedback,                      METH_VARARGS, 0 },
    { "pmgr_getNumberOfPlayers",     pmgr_getNumberOfPlayers,            METH_VARARGS, 0 },
    { "pmgr_getPlayers",             pmgr_getPlayers,                    METH_VARARGS, 0 },
    { "pmgr_getCommander",           pmgr_getCommander,                  METH_VARARGS, 0 },
    { "pmgr_isIndexValid",           pmgr_isIndexValid,                  METH_VARARGS, 0 },
    { "pmgr_p_get",                  pmgr_p_get,                         METH_VARARGS, 0 },
    { "pmgr_p_set",                  pmgr_p_set,                         METH_VARARGS, 0 },
    { "pmgr_getScore",               pmgr_getScore,                      METH_VARARGS, 0 },
    { "pmgr_setScore",               pmgr_setScore,                      METH_VARARGS, 0 },
    { "pmgr_enableScoreEvents",      pmgr_enableScoreEvents,             METH_VARARGS, 0 },
    { "timer_getWallTime",           timer_getWallTime,                  METH_VARARGS, 0 },
    { "timer_getTimers",             timer_getTimers,                    METH_VARARGS, 0 },
    { "timer_created",               timer_created,                      METH_VARARGS, 0 },
    { "timer_destroy",               timer_destroy,                      METH_VARARGS, 0 },
    { "registerHandler",             registerHandler,                    METH_VARARGS, 0 },
    { "registerGameStatusHandler",   registerGameStatusHandler,          METH_VARARGS, 0 },
    { "unregisterGameStatusHandler", unregisterGameStatusHandler,        METH_VARARGS, 0 },
    { "omgr_getObjectsOfType",       omgr_getObjectsOfType,              METH_VARARGS, 0 },
    { "omgr_getObjectsOfTemplate",   omgr_getObjectsOfTemplate,          METH_VARARGS, 0 },
    { "sgl_getModDirectory",         sgl_getModDirectory,                METH_VARARGS, 0 },
    { "sgl_getMapName",              sgl_getMapName,                     METH_VARARGS, 0 },
    { "sgl_getWorldSize",            sgl_getWorldSize,                   METH_VARARGS, 0 },
    { "sgl_setParam",                sgl_setParam,                       METH_VARARGS, 0 },
    { "sgl_getParam",                sgl_getParam,                       METH_VARARGS, 0 },
    { "sgl_endGame",                 sgl_endGame,                        METH_VARARGS, 0 },
    { "sgl_getControlPointsInGroup", sgl_getControlPointsInGroup,        METH_VARARGS, 0 },
    { "sgl_getIsAIGame",             sgl_getIsAIGame,                    METH_VARARGS, 0 },
    { "sgl_sendGameLogicEvent",      sgl_sendGameLogicEvent,             METH_VARARGS, 0 },
    { "sgl_sendPythonEvent",         sgl_sendPythonEvent,                METH_VARARGS, 0 },
    { "sgl_sendMedalEvent",          sgl_sendMedalEvent,                 METH_VARARGS, 0 },
    { "sgl_sendRankEvent",           sgl_sendRankEvent,                  METH_VARARGS, 0 },
    { "sgl_sendHudEvent",            sgl_sendHudEvent,                   METH_VARARGS, 0 },
    { "sgl_sendTextMessage",         sgl_sendTextMessage,                METH_VARARGS, 0 },
    { "gl_sendEndOfRoundData",       gl_sendEndOfRoundData,              METH_VARARGS, 0 },
    { "ss_getParam",                 ss_getParam,                        METH_VARARGS, 0 },
    { "sgl_getSettingsBool",         sgl_getSettingsBool,                METH_VARARGS, 0 },
    { "sh_setEnableCommander",       sh_setEnableCommander,              METH_VARARGS, 0 },
    { "pers_plrAwardMedal",          pers_plrAwardMedal,                 METH_VARARGS, 0 },
    { "pers_plrSetUnlocks",          pers_plrSetUnlocks,                 METH_VARARGS, 0 },
    { "pers_plrRequestUnlocks",      pers_plrRequestUnlocks,             METH_VARARGS, 0 },
    { "pers_plrRequestStats",        pers_plrRequestStats,               METH_VARARGS, 0 },
    { "pers_plrRequestAwards",       pers_plrRequestAwards,              METH_VARARGS, 0 },
    { "pers_getStatsKeyVal",         pers_getStatsKeyVal,                METH_VARARGS, 0 },
    { "pers_gamespyStatsSendSnapshotUpdate", pers_gamespyStatsSendSnapshotUpdate, METH_VARARGS, 0 },
    { "pers_gamespyStatsSendSnapshotFinal",  pers_gamespyStatsSendSnapshotFinal,  METH_VARARGS, 0 },
    { "pers_gamespyStatsEndGame",    pers_gamespyStatsEndGame,           METH_VARARGS, 0 },
    { "pers_gamespyStatsNewGame",    pers_gamespyStatsNewGame,           METH_VARARGS, 0 },
    { "trig_create",                 trig_create,                        METH_VARARGS, 0 },
    { "trig_getObjects",             trig_getObjects,                    METH_VARARGS, 0 },
    { "trig_destroy",                trig_destroy,                       METH_VARARGS, 0 },
    { "trig_destroyAll",             trig_destroyAll,                    METH_VARARGS, 0 },
    { 0, 0, 0, 0 }                                       // the NULL-name terminator
};

// Append Python output only while an exception capture stream is installed.
// Ordinary script output is discarded. Return a new reference to None on success;
// argument conversion failure preserves the Python exception and returns null.
PyObject* log_write(PyObject*, PyObject* args)
{
    std::string text;
    if (py_arg<std::string>(args, 0, text))          // 1 == failed
        return 0;                                    // the exception is already set

    PythonHostData* const d = pyHostData();
    if (d->m_captureStream)                          // +0x34
    {
        // Without an active capture stream, the hook discards output.
        *d->m_captureStream << text << '\n';
    }

    Py_INCREF(Py_None);
    return Py_None;
}

void createHostModule()
{
    PythonHostData* const d = pyHostData();

    d->m_hostModule = Py_InitModule4(const_cast<char*>("host"), method_table,
                                     0, 0, PYTHON_API_VERSION);   // 1012 for 2.3
    if (!d->m_hostModule || PyErr_Occurred())
    {
        Debug(DEBUGTYPE_6, "Game/Python/PythonHost.cpp", 175)
            << "host module creation failed: " << pythonHost()->getExceptionText();
        d->m_hostModule = 0;                             // unreachable - the Debug aborts
    }
}

// Initialize Python once, normalize the mod path, and create the host module.
// [UNRESOLVED name] minimal skips the bf2 import, object bindings, event table
// and event registration. Normal startup passes false. Script paths include
// relative python/admin directories, so the process working directory matters.
// A failing bf2.init_module produces a fatal diagnostic.
bool PythonHost::initialize(bool minimal)
{
    PythonHostData* const d = m_data;

    if (d->m_initialized)                                // +0x00 - already up
        return false;

    d->m_byte01 = minimal;                               // +0x01, cached for reinitialize()

    // Skip import site; the game installs its own Python module search path.
    Py_NoSiteFlag = 1;

    Py_Initialize();
    // Install the game module search path.
    std::string modDir = settings->stringGet(std::string("GSModDirectory"), std::string());
    for (std::string::size_type i = 0; i < modDir.length(); ++i)
        if (modDir[i] == '\\')                           // unconditional normalisation
            modDir[i] = '/';

    std::string extra;
    if (!modDir.empty() && modDir.compare("mods/bf2") != 0)
        extra = "'" + modDir + "/python' , ";            // omitted for a stock bf2 server

    const std::string script =
        std::string("import sys\nsys.path = [ 'pylib-2.3.4.zip', '") + "python" + "' , "
        + extra + "'mods/bf2/python' , 'admin' ]";
    PyRun_SimpleString(const_cast<char*>(script.c_str()));

    createHostModule();

    // everything below IS skipped when `minimal` IS SET - see the block above.
    if (!minimal)
    {
        // host.Object
        PyObject* const className = PyString_FromString("Object");
        PyObject* const classDict = PyDict_New();
        d->m_objectClass = PyClass_New(0, classDict, className);          // +0x10
        if (!d->m_objectClass)
        {
            Debug(DEBUGTYPE_6, "Game/Python/PythonHost.cpp", 250)
                << "failed to create host.Object class type";
            return false;                                // unreachable - the Debug aborts
        }

        // import bf2
        d->m_bf2Module = PyImport_ImportModule(const_cast<char*>("bf2"));  // +0x04
        if (!d->m_bf2Module)
        {
            Debug(DEBUGTYPE_6, "Game/Python/PythonHost.cpp", 257)
                << "couldn't import the bf2 module: " << getExceptionText();
            return false;
        }

        // THE magic 42 - see the block above.
        d->m_vector28.resize(42);                        // +0x28, indexed by ScriptEvent

        PyObject* const bf2Dict = PyModule_GetDict(d->m_bf2Module);

        // bf2.playerConvFunc
        d->m_playerConvFunc = PyDict_GetItemString(bf2Dict, "playerConvFunc");   // +0x0C
        if (!PyCallable_Check(d->m_playerConvFunc))
        {
            Debug(DEBUGTYPE_6, "Game/Python/PythonHost.cpp", 271)
                << "IPlayer_to_python not callable";
            d->m_playerConvFunc = 0;
        }

        // bf2.init_module()
        PyObject* const initModule = PyDict_GetItemString(bf2Dict, "init_module");
        if (initModule && PyCallable_Check(initModule))
        {
            PyObject* const result = PyObject_CallObject(initModule, 0);
            if (result)
            {
                Py_DECREF(result);
            }
            else
            {
                // type 7, NOT 6 - but 7 folds to 6 in ~Debug, so it still aborts.
                Debug(DEBUGTYPE_7, "Game/Python/PythonHost.cpp", 281)
                    << "bf2 init_module failed: " << getExceptionText();
            }
        }

        // the matching unregister is in shutdown() - see PythonHost's constructor note.
        eventManager->registerEventHandler(static_cast<EventCategory>(0), this, 0);
    }

    d->m_initialized = true;                             // +0x00
    return true;
}


// . IT does NOT read A member OF PythonHost - it dereferences the pimpl and reads
// its first byte, `movzx eax, byte ptr [eax]` after `mov eax, [this+4]`.
bool PythonHost::isInitialized() const
{
    return m_data->m_initialized;                            // PythonHostData +0x00
}

// Shut down the admin module while its handles are live, release callbacks and
// queues, unregister events, then finalize Python. Run this before destroying
// the host. m_objectClass and m_gameModeDeinit are left for the destructor; a
// failed game-mode deinit may retain its reference through finalization.
void PythonHost::shutdown()
{
    if (m_data->m_dword44)                                   // +0x44
    {
        PyObject* result = PyObject_CallFunction(m_data->m_dword44, 0);
        if (!result)
        {
            Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 491)
                << "failed to shut down admin module: " << getExceptionText();
        }
        Py_XDECREF(result);
    }

    m_data->m_timers.clear();                                // +0x18
    m_data->m_vector28.erase(m_data->m_vector28.begin(), m_data->m_vector28.end());   // +0x28

    PyObject* old = m_data->m_bf2Module;                     // +0x04  PyRef = 0
    m_data->m_bf2Module = 0;
    Py_XDECREF(old);
    old = m_data->m_hostModule;                              // +0x08
    m_data->m_hostModule = 0;
    Py_XDECREF(old);
    old = m_data->m_playerConvFunc;                          // +0x0C
    m_data->m_playerConvFunc = 0;
    Py_XDECREF(old);

    m_data->m_list14.resize(0, PyRef());                     // +0x14
    m_data->m_triggers.clear();                              // +0x38
    m_data->m_triggerCursor = m_data->m_triggers.begin();    // +0x3C
    m_data->m_initialized = false;                           // +0x00

    old = m_data->m_adminScriptUpdate;                       // +0x40
    m_data->m_adminScriptUpdate = 0;
    Py_XDECREF(old);
    old = m_data->m_dword44;                                 // +0x44
    m_data->m_dword44 = 0;
    Py_XDECREF(old);

    eventManager->unregisterEventHandler(static_cast<EventCategory>(0), this);   // vt +0x30
    if (Py_IsInitialized())
        Py_Finalize();
}

//  / PE 0x42A370. Import, retain init/shutdown/update in that order, validate all
// three, call init without arguments, and publish update then shutdown only on success.
// Borrowed dictionary entries are retained across init(), which may replace those entries.
bool PythonHost::initializeAdminMod(const std::string& moduleName)
{
    PyObject* module = PyImport_ImportModule(const_cast<char*>(moduleName.c_str()));
    if (!module)
    {
        Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 302)
            << "admin: failed to import " << moduleName << ": " << getExceptionText();
        return false;
    }
    PyObject* dict = PyModule_GetDict(module);
    bool success = false;
    {
        PyRef init(PyDict_GetItemString(dict, const_cast<char*>("init")));
        PyRef shutdown(PyDict_GetItemString(dict, const_cast<char*>("shutdown")));
        PyRef update(PyDict_GetItemString(dict, const_cast<char*>("update")));
        if (!init || !shutdown || !update || !PyCallable_Check(init)
            || !PyCallable_Check(shutdown) || !PyCallable_Check(update))
            Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 314)
                << "admin: " << moduleName
                << ": you must provide the functions init, shutdown and update -- can't enable admin";
        else
        {
            PyObject* result = PyObject_CallFunction(init, 0);
            if (!result)
                Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 320)
                    << "admin: " << moduleName << ": init function failed: " << getExceptionText();
            else
            {
                Py_XDECREF(m_data->m_adminScriptUpdate);
                m_data->m_adminScriptUpdate = update.get();
                Py_XINCREF(m_data->m_adminScriptUpdate);
                Py_XDECREF(m_data->m_dword44);
                m_data->m_dword44 = shutdown.get();
                Py_XINCREF(m_data->m_dword44);
                success = true;
            }
            Py_XDECREF(result);
        }
    } // release update, shutdown, init, then the imported module (retail PyRef destruction order)
    Py_DECREF(module);
    return success;
}

void PythonHost::update()
{
    if (!m_data->m_initialized)
        return;

    if (m_data->m_adminScriptUpdate)
    {
        // Format string NULL - no arguments. `PyObject_CallFunction(fn, 0)` at 0x815FDBB.
        PyObject* result = PyObject_CallFunction(m_data->m_adminScriptUpdate, 0);

        if (!result)
        {
            // The binary passes the four-argument form with an explicitly built empty function name,
            // which is what Debug's `const std::string& func = ""` default constructs anyway.
            Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 551)
                << "error: admin update failed: " << getExceptionText();

            // AND THE callback IS dropped. See the header block - this is not tidy-up, it is the
            // policy: one failure retires the hook.
            Py_XDECREF(m_data->m_adminScriptUpdate);
            m_data->m_adminScriptUpdate = 0;
        }

        Py_XDECREF(result);
    }

    if (!m_data->m_byte4C)
    {
        updateTimers();                                      // always first
        updateTriggers();
    }
}

// [UNRESOLVED] home. `python::extractField<double>` (0x8162B60) and `<int>` (0x8162C00) sit past this
// file's range, in the TU that also holds the predicators. Declared here because updateTimers is the
// only recovered caller; the mangled names give the signature exactly - bool(PyObject*, const char*,
// T&).
template <class T> bool extractField(PyObject* object, const char* name, T& out);

// Reads a Python attribute unless an error is already pending, and releases its
// reference after conversion. double accepts integers or floats; int requires an
// exact PyInt. Missing objects raise an error.
template <> bool extractField<double>(PyObject* object, const char* name, double& out)
{
    if (PyErr_Occurred())
        return false;
    if (!object)
    {
        PyErr_SetString(PyExc_RuntimeError, "no object");
        return false;
    }

    PyObject* const attr = PyObject_GetAttrString(object, const_cast<char*>(name));
    if (!attr)
        return false;

    const int failed = parse_arg(attr, out);             // the double form - accepts ints too
    Py_DECREF(attr);
    return !failed;
}

template <> bool extractField<int>(PyObject* object, const char* name, int& out)
{
    if (PyErr_Occurred())
        return false;
    if (!object)
    {
        PyErr_SetString(PyExc_RuntimeError, "no object");
        return false;
    }

    PyObject* const attr = PyObject_GetAttrString(object, const_cast<char*>(name));
    if (!attr)
        return false;

    bool failed;
    if (attr->ob_type == &PyInt_Type || PyType_IsSubtype(attr->ob_type, &PyInt_Type))
    {
        out    = PyInt_AsLong(attr);
        failed = PyErr_Occurred() != 0;
    }
    else
    {
        failed = true;
        PyErr_SetString(PyExc_RuntimeError, "expected an integer");
    }

    Py_DECREF(attr);
    return !failed;
}


// Fires due Python timers during active play unless alwaysTrigger is positive.
// Missing time/interval attributes, callback exceptions or failed time writes silently remove a timer.
// Positive intervals reschedule once; missed intervals do not cause catch-up callbacks.
// Removals requested during callbacks are deferred until the sweep ends, so a later timer
// can still fire in the same frame after cancellation.
void PythonHost::updateTimers()
{
    // A local std::set, header node allocate(20) at 0x815FF3F. See the header block - what it is for
    // and why it achieves nothing.
    std::set<PyRef> rescheduled;

    const double now = g_system->getExactTime();             // ds:0x8AC0D20, vcall +0x18 at 0x815FF76

    std::set<PyRef>::iterator it = m_data->m_timers.begin();
    while (it != m_data->m_timers.end())
    {
        double triggerTime   = 0.0;
        double interval      = 0.0;
        int    alwaysTrigger = 0;
        bool   drop          = false;

        // The set holds PyRef, so the local copy takes its own reference for the duration - the
        // callback below can and does mutate the set.
        PyRef timer(*it);

        if (!extractField<double>(timer, "time", triggerTime))
            drop = true;
        if (!extractField<double>(timer, "interval", interval))
            drop = true;
        if (!extractField<int>(timer, "alwaysTrigger", alwaysTrigger))
            alwaysTrigger = 0;                               // the only forgiving one

        if (!drop && now >= triggerTime)
        {
            if (alwaysTrigger > 0 ||
                (gameLogic &&
                 gameLogic->getGameStatus() == GameStatus_Playing &&
                 static_cast<int>(gameLogic->getBeginRoundState()) == BeginRoundState_Running))
            {
                PyObject* result = PyObject_CallMethod(timer, (char*)"onTrigger", 0);
                if (result)
                {
                    Py_DECREF(result);
                }
                else
                {
                    PyErr_Clear();                           // swallowed, not printed
                    drop = true;
                }
            }

            if (drop || interval <= 0.0)
            {
                drop = true;                                 // one-shot, or the callback failed
            }
            else
            {
                PyObject* next = PyFloat_FromDouble(interval + now);

                // Reschedule from now, not the previous deadline; delayed timers drift instead of catching up.
                if (PyObject_SetAttrString(timer, (char*)"time", next) == -1)
                {
                    PyErr_Clear();
                    drop = true;
                }
                else
                {
                    rescheduled.insert(timer);
                }
            }
        }

        if (drop)
            m_data->m_timers.erase(it++);                    // advance first, then rebalance
        else
            ++it;
    }

    // No-op by construction - see the header block. Kept as written.
    m_data->m_timers.insert(rescheduled.begin(), rescheduled.end());

    // The deferred cancellations removeTimer() queued, drained only now.
    for (std::list<PyRef>::iterator dead = m_data->m_timersToRemove.begin();
         dead != m_data->m_timersToRemove.end(); ++dead)
    {
        m_data->m_timers.erase(*dead);
    }
    m_data->m_timersToRemove.clear();
}

// insertTimer / removeTimer - ELF 0x8160420 / 0x8160490. Both bodies are a single source-level STL
// operation around the refcount-owning PyRef temporary. Removal is deliberately deferred until the
// end of updateTimers, so cancelling one timer from another callback does not perturb this sweep.
void PythonHost::insertTimer(PyObject* timer)
{
    m_data->m_timers.insert(PyRef(timer));
}

void PythonHost::removeTimer(PyObject* timer)
{
    m_data->m_timersToRemove.push_back(PyRef(timer));
}

// Trigger updates use a five-iteration round-robin budget, including the single allowed wrap.
// Dead source objects remove their triggers. Exit callbacks may receive a null departing object;
// control-point soldier filtering applies only to entry callbacks.
// Trigger creation and removal establish the iterator used by the sweep.
void getAllPlayers(world::IObject* obj, std::list<world::IPlayer*>& out);

// Invokes the trigger callback with id, source object, crossing object, enter flag,
// and optional user data. Unless alwaysTrigger is set, waits for the playing round
// and begin-round state 3. Callback exceptions are cleared without logging.
void PythonHost::fireTriggerEvent(RadiusTrigger& trigger, TriggerMode mode, world::IObject* object)
{
    int argc = 4;
    if (trigger.m_userData.get())
        argc = 5;

    const bool allowed =
        trigger.m_alwaysTrigger != 0
        || (gameLogic
            && gameLogic->getGameStatus() == GameStatus_Playing
            && static_cast<int>(gameLogic->getBeginRoundState()) == BeginRoundState_Running);
    if (!allowed)
        return;

    PyObject* const args = PyTuple_New(argc);

    PyTuple_SetItem(args, 0, PyInt_FromLong(trigger.m_id));                    // steals

    world::IObject* const source =
        trigger.m_source.m_ctrl ? trigger.m_source.m_ctrl->target : 0;
    PyTuple_SetItem(args, 1, wrapObject(source));                             // steals
    PyTuple_SetItem(args, 2, wrapObject(object));                             // steals
    PyTuple_SetItem(args, 3, PyInt_FromLong(mode == 0));                      // "is enter"

    if (argc == 5)
    {
        PyObject* const data = trigger.m_userData.get();
        Py_INCREF(data);                                                      // SetItem steals
        PyTuple_SetItem(args, 4, data);
    }

    PyObject* const result = PyObject_CallObject(trigger.m_callback.get(), args);
    if (!result)
        PyErr_Clear();                       // swallowed, as shipped
    Py_XDECREF(result);
    Py_DECREF(args);
}

// Grid predicates use full hierarchy culling without flag filtering. PCO triggers
// require both the interface and at least one player in the object's subtree;
// empty vehicles do not qualify.
bool TemplatePredicator::predicate(world::IObject* object)
{
    // the object's +0x14 template against the one trig_create stored on the trigger
    return world::object_detail::LayoutAccess::objectTemplate(object) == m_objectTemplate;
}

unsigned int TemplatePredicator::getFlags() const     { return 0; }
bool TemplatePredicator::performCulling(world::IObject*) { return true; }

bool PCOPredicator::predicate(world::IObject* object)
{
    if (!object->queryInterface(world::IID_IPlayerControlObject))
        return false;

    std::list<world::IPlayer*> players;
    getAllPlayers(object, players);
    return !players.empty();                 // occupied, not merely a PCO - see above
}

unsigned int PCOPredicator::getFlags() const          { return 0; }
bool PCOPredicator::performCulling(world::IObject*)   { return true; }

void PythonHost::updateTriggers()
{
    std::list<RadiusTrigger>& triggers = m_data->m_triggers;

    if (triggers.empty())
        return;

    // Sixteen null elements, allocated once and reused. `allocate(64)` + `fill_n(.., 16, NULL)` at
    //  / 0x8160584, cleared again at the top of every pass.
    std::vector<world::IObject*> objects(16);

    // Both constructed once, here, outside the loop. TemplatePredicator's field is re-assigned per
    // trigger below.
    TemplatePredicator templatePredicator;
    PCOPredicator      pcoPredicator;

    int  remaining = 5;                                      // the per-frame budget
    bool wrapped   = false;                                  // and the once-only wrap

    for (;;)
    {
        if (m_data->m_triggerCursor == triggers.end())
        {
            if (wrapped)
                break;                                       // second time round: done for this frame
            m_data->m_triggerCursor = triggers.begin();
            wrapped = true;
        }

        if (--remaining < 0)
            break;

        // Re-tested after the decrement, which is why reaching the end costs a budget unit.
        if (m_data->m_triggerCursor == triggers.end())
            continue;

        RadiusTrigger& trigger = *m_data->m_triggerCursor;
        templatePredicator.m_objectTemplate = trigger.m_objectTemplate;

        world::IObject* source = trigger.m_source.get();
        if (!source)
        {
            // The source object is gone. Advance first, then erase - the cursor must not be left
            // pointing at a freed node.
            std::list<RadiusTrigger>::iterator dead = m_data->m_triggerCursor;
            ++m_data->m_triggerCursor;
            triggers.erase(dead);
            continue;
        }

        Vec3 centre = source->getAbsolutePosition();         // vcall +0x6C
        Mat4 xform  = source->getAbsoluteTransformation();   // vcall +0x84, all 16 floats copied

        bool soldiersOnly = false;

        const world::IObjectTemplate* objectTemplate = source->getTemplate();
        if (objectTemplate->getClassID() == world::CID_ControlPointTemplate)
        {
            world::IControlPointTemplate* controlPoint =
                static_cast<world::IControlPointTemplate*>(
                    objectTemplate->queryInterface(world::IID_IControlPointTemplate));

            if (controlPoint)
            {
                soldiersOnly = controlPoint->getForSoldierOnly();          // +0x100 (real name)

                const Vec3 offset = controlPoint->getRadiusOffset();       // +0xB4  (real name)
                centre.x += offset.x;
                centre.y += offset.y;
                centre.z += offset.z;

                // asked A second time, added to the transform's translation row - and nothing
                // below reads that row. Dead as written (0x8160D17); kept because removing it also
                // removes a second virtual call the original really makes.
                const Vec3 offsetAgain = controlPoint->getRadiusOffset();
                xform.m[12] += offsetAgain.x;
                xform.m[13] += offsetAgain.y;
                xform.m[14] += offsetAgain.z;
            }
        }

        objects.clear();
        // Use the collision-radius query so trigger checks include soldiers and vehicles.
        world::objectManager->getObjectsFromGrid(
            centre, trigger.m_radius, objects,
            trigger.m_onlyPCO ? static_cast<world::ObjectPredicator*>(&pcoPredicator)
                              : static_cast<world::ObjectPredicator*>(&templatePredicator));

        if (trigger.m_cullBehind)
        {
            // A half-space test, not a second radius: anything on the negative side of the source's
            // second transform axis is dropped, which turns the sphere into a hemisphere. The axis is
            // m[4..6] - row 1 of the matrix as copied.
            for (std::vector<world::IObject*>::size_type i = 0; i < objects.size(); )
            {
                const Vec3& p = objects[i]->getAbsolutePosition();
                const float dx = p.x - centre.x;
                const float dy = p.y - centre.y;
                const float dz = p.z - centre.z;

                if (dx * xform.m[4] + dy * xform.m[5] + dz * xform.m[6] < 0.0f)
                    objects.erase(objects.begin() + i);
                else
                    ++i;
            }
        }

        // Weak handles, so that an object destroyed between two sweeps is still identifiable when it
        // shows up in the exit set.
        std::set<WeakPtr<world::IObject> > inside;
        for (std::vector<world::IObject*>::size_type i = 0; i < objects.size(); ++i)
            inside.insert(objects[i]->getWeakPtr());

        std::list<WeakPtr<world::IObject> > changed;
        std::set_symmetric_difference(inside.begin(), inside.end(),
                                      trigger.m_inside.begin(), trigger.m_inside.end(),
                                      std::back_inserter(changed));

        std::list<WeakPtr<world::IObject> > entered;
        std::set_difference(changed.begin(), changed.end(),
                            trigger.m_inside.begin(), trigger.m_inside.end(),
                            std::back_inserter(entered));

        std::list<WeakPtr<world::IObject> > exited;
        std::set_difference(changed.begin(), changed.end(),
                            inside.begin(), inside.end(),
                            std::back_inserter(exited));

        trigger.m_inside = inside;                           // _Rb_tree::operator= at 0x8160A78

        // enter first, exit second. Both before the cursor advances, so a callback that destroys the
        // trigger it was fired from is destroying the node this loop is still holding a reference
        // into. Not guarded in the original.
        for (std::list<WeakPtr<world::IObject> >::iterator it = entered.begin();
             it != entered.end(); ++it)
        {
            world::IObject* object = it->get();
            if (object)
            {
                if (!soldiersOnly ||
                    object->getTemplate()->getClassID() == world::CID_SoldierTemplate)
                {
                    // TriggerMode 0. fireTriggerEvent turns it into the callback's fourth argument as
                    // `PyInt_FromLong(mode == 0)` (0x8161FC1), so 0 IS "entered" and the script sees
                    // a bool. [UNRESOLVED] enumerator names; the values are read.
                    fireTriggerEvent(trigger, (TriggerMode)0, object);
                }
            }
        }

        for (std::list<WeakPtr<world::IObject> >::iterator it = exited.begin();
             it != exited.end(); ++it)
        {
            // NO NULL check, AND NO soldier filter. Both deliberate - see the header block.
            fireTriggerEvent(trigger, (TriggerMode)1, it->get());
        }

        // [PLATFORM] A Python callback may destroy this trigger (or all triggers), and both host
        // wrappers then reset the shared cursor to begin(). Retail's list sentinel advances from
        // end() back to itself; a checked modern MSVC iterator rejects ++end(). The guard preserves
        // the retail cursor state while avoiding that library-version exception.
        if (m_data->m_triggerCursor != triggers.end())
            ++m_data->m_triggerCursor;
    }
}

// Synchronously reconciles all radius triggers for a nonnull object. Radius equality
// counts as outside; rejected hemisphere points retain their prior membership.
// Dead trigger sources are skipped. Callbacks may mutate the trigger list during iteration.
void PythonHost::updateTriggersFromObject(world::IObject* object)
{
    const Vec3& objectPosition = object->getAbsolutePosition();

    for (std::list<RadiusTrigger>::iterator it = m_data->m_triggers.begin();
         it != m_data->m_triggers.end(); ++it)
    {
        RadiusTrigger& trigger = *it;
        world::IObject* const source = trigger.m_source.get();
        if (!source)
            continue;

        const Vec3& sourcePosition = source->getAbsolutePosition();
        const float dx = objectPosition.x - sourcePosition.x;
        const float dy = objectPosition.y - sourcePosition.y;
        const float dz = objectPosition.z - sourcePosition.z;
        const float distanceSquared = dx * dx + dy * dy + dz * dz;

        if (distanceSquared >= trigger.m_radius * trigger.m_radius)
        {
            std::set<WeakPtr<world::IObject> >::iterator inside =
                trigger.m_inside.find(object->getWeakPtr());
            if (inside != trigger.m_inside.end())
            {
                trigger.m_inside.erase(inside);
                fireTriggerEvent(trigger, static_cast<TriggerMode>(1), object);
            }
            continue;
        }

        if (trigger.m_cullBehind)
        {
            const Mat4& transform = source->getAbsoluteTransformation();
            if (dx * transform.m[4] + dy * transform.m[5] + dz * transform.m[6] < 0.0f)
                continue;
        }

        if (trigger.m_inside.find(object->getWeakPtr()) == trigger.m_inside.end())
        {
            trigger.m_inside.insert(object->getWeakPtr());
            fireTriggerEvent(trigger, static_cast<TriggerMode>(0), object);
        }
    }
}

bool PythonHost::initializeGameMode(const std::string& gameMode)
{
    if (gameMode.empty())                                    // the length gate
        return false;

    // Lowercase and whitelist the game-mode name before constructing a Python command.
    std::string mode = getLowerCase(gameMode);
    std::string charset("abcdefghijklmnopqrstuvwxyz0123456789_");
    for (unsigned int i = 0; i < mode.length(); ++i)
    {
        if (charset.find(mode[i], 0) == std::string::npos)
            return false;
    }

    // Probe python/game/gamemodes/<mode>.py through the FileManager (so archives count); a missing
    // file falls back to gpm_cq instead of failing.
    std::ostringstream pathStream;
    pathStream << "python/game/gamemodes/" << mode << ".py";
    std::string modeName;
    if (!io::fileManager->fileExists(pathStream.str()))
        modeName = "gpm_cq";
    else
        modeName = mode;

    // The composed-and-discarded script text - dead code, preserved as found (see the corrected
    // header note: no PyRun_* consumes it; the C API below does the work).
    std::ostringstream deadScript;
    deadScript << "import game.gamemodes." << modeName << "\n";
    deadScript << "game.gamemodes." << modeName << ".init()\n";

    std::string moduleName = std::string("game.gamemodes.") + modeName;
    PyObject* module = PyImport_ImportModule(const_cast<char*>(moduleName.c_str()));
    if (!module)
    {
        Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 369)
            << "couldn't import game mode " << modeName << ": " << getExceptionText();
        return false;
    }

    PyObject* dict = PyModule_GetDict(module);               // borrowed
    PyObject* init = PyDict_GetItemString(dict, "init");     // borrowed
    Py_XINCREF(init);                                        // the inlined PyRef ctor's incref
    if (!init)
    {
        Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 377)
            << "python game mode " << modeName << " doesn't have an init function";
        Py_XDECREF(module);
        return false;
    }

    PyObject* result = PyObject_CallFunction(init, 0);
    if (!result)
    {
        Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 383)
            << "python game mode " << modeName << " failed to initialize: " << getExceptionText();
        Py_XDECREF(init);
        Py_XDECREF(module);
        return false;
    }

    // Bind the module's deinit into the pimpl (+0x48) with PyRef-assignment refcount semantics:
    // net +1 on the new borrowed ref, -1 on whatever was there.
    {
        PyObject* deinit = PyDict_GetItemString(dict, "deinit");   // borrowed
        Py_XINCREF(deinit);
        PyObject* old = m_data->m_gameModeDeinit;
        m_data->m_gameModeDeinit = deinit;
        Py_XDECREF(old);
    }

    Py_XDECREF(result);
    Py_XDECREF(init);
    Py_XDECREF(module);
    return true;
}

// Calls the game mode's deinitializer. Releases it only on success; exceptions are
// logged nonfatally and leave the callable available for another unload attempt.
void PythonHost::uninitializeGameMode()
{
    if (!m_data->m_gameModeDeinit)                           // +0x48
        return;

    PyObject* result = PyObject_CallFunction(m_data->m_gameModeDeinit, 0);
    if (result)
    {
        PyObject* old = m_data->m_gameModeDeinit;            // PyRef::operator=(0)
        m_data->m_gameModeDeinit = 0;
        Py_XDECREF(old);
    }
    else
    {
        Debug(DEBUGTYPE_8, "Game/Python/PythonHost.cpp", 399)
            << "python game mode failed to deinitialize: " << getExceptionText();
    }
    Py_XDECREF(result);
}

// Captures and clears the pending Python exception through the host's output hook.
// The temporary output stream is borrowed only during PyErr_Print.
std::string PythonHost::getExceptionText() const
{
    std::ostringstream out;
    m_data->m_captureStream = &out;                          // +0x34
    PyErr_Print();
    m_data->m_captureStream = 0;
    return out.str();
}

} // namespace python
} // namespace hfe
} // namespace dice
