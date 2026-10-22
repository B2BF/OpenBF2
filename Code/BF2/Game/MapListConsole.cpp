// The maplist console object: append, list and currentMap. Entry insertion still obeys
// MapList's validation and modifiable rules; the read commands serve the rotation text and
// selection that level scripts and server tooling consume. [CHOSEN] Binding file placement;
// access level is end-user.

#include "../Core/ConsoleObjectMacro.h"
#include "MapList.h"
#include <string>

namespace dice {
namespace hfe {

namespace {

// Stands in for the original's file-scope list, the way kSvConsoleTag does for sv.
const char* const kMapListConsoleTag = "hfe.maplist";

// Appends only while the map list is modifiable; isObjectActive enforces this
// precondition. The receiver is not null-checked and the override flag stays false.
bool mapListAppend(const std::string& name, const std::string& gameMode, int mapSize)
{
    return MapList::consoleWriteAccess()->appendEntry(name, gameMode, mapSize, false);
}

// maplist.list - the rotation as console text (ELF ConsoleClass761::executeObjectMethod at
// 0xa91d4b calls formatListForConsole(false)).
std::string mapListList()
{
    return MapList::consoleReadAccess()->formatListForConsole(false);
}

// maplist.currentMap - the selected rotation index, or -1 for none (ELF getCurrentMap 0x4173f0).
int mapListCurrentMap()
{
    return MapList::consoleReadAccess()->getCurrentMap();
}

// maplist.load - reloads the configured rotation script; false when no config path is set.
bool mapListLoad()
{
    return MapList::consoleWriteAccess()->load(false);
}

// maplist.save - writes the rotation back through the configuration stream.
bool mapListSave()
{
    return MapList::consoleReadAccess()->save();
}

// maplist.mapCount - number of entries in the rotation.
int mapListMapCount()
{
    return MapList::consoleReadAccess()->getMapCount();
}

// maplist.clear - empties the rotation and resets the selection to none.
bool mapListClear()
{
    return MapList::consoleWriteAccess()->clear(false);
}

// maplist.remove - drops one entry by index; the console result is 1 on success, 0 otherwise.
int mapListRemove(int idx)
{
    return MapList::consoleWriteAccess()->removeEntry(idx, false) ? 1 : 0;
}

// maplist.insert - inserts before an existing index; the map size defaults to zero.
bool mapListInsert(int idx, const std::string& name, const std::string& gameMode, int mapSize)
{
    return MapList::consoleWriteAccess()->insertEntry(idx, name, gameMode, mapSize, false);
}

// maplist.configFile - the rotation filename, read or replaced on the live list.
std::string mapListGetConfigFile()
{
    return MapList::consoleReadAccess()->m_configFile;
}

void mapListSetConfigFile(const std::string& path)
{
    MapList::consoleReadAccess()->m_configFile = path;
}

} // namespace

// SetConsoleClassDefaults("maplist", 1, 0) - see the header block for where both values are read.
BF2_CONSOLE_OBJECT("maplist", 1, 0)

// mapList.append takes a map, mode, and optional size, returning success.
// Omitting the size records zero; it does not select a default map size.
BF2_CONSOLE_METHOD_2_3_RET_ACTIVE(append, mapListAppend, bool,
                                  MapList::consoleWriteAccess() != 0,
                                  std::string, std::string, int, 0,
                                  kMapListConsoleTag)

BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, mapListList, std::string,
                                MapList::consoleReadAccess() != 0, kMapListConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(currentMap, mapListCurrentMap, int,
                                MapList::consoleReadAccess() != 0, kMapListConsoleTag)

// maplist.load runs through the write receiver, maplist.save through the read receiver,
// matching the shipped executors' gates.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(load, mapListLoad, bool,
                                MapList::consoleWriteAccess() != 0, kMapListConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, mapListSave, bool,
                                MapList::consoleReadAccess() != 0, kMapListConsoleTag)

// maplist.configFile - set or read the rotation filename.
BF2_CONSOLE_PROPERTY_FN(configFile, std::string, MapList::consoleReadAccess() != 0,
                        mapListGetConfigFile(), mapListSetConfigFile, kMapListConsoleTag)

// maplist.mapCount - entry count, gated on the read receiver.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(mapCount, mapListMapCount, int,
                                MapList::consoleReadAccess() != 0, kMapListConsoleTag)

// maplist.clear - empties the rotation through the write receiver.
BF2_CONSOLE_METHOD_0_RET_ACTIVE(clear, mapListClear, bool,
                                MapList::consoleWriteAccess() != 0, kMapListConsoleTag)

// maplist.remove - drops an entry by index; success is reported as 1.
BF2_CONSOLE_METHOD_1_RET_ACTIVE(remove, mapListRemove, int,
                                MapList::consoleWriteAccess() != 0, int, kMapListConsoleTag)

// maplist.insert - index, map, mode and optional size; the size defaults to zero.
BF2_CONSOLE_METHOD_3_4_RET_ACTIVE(insert, mapListInsert, bool,
                                  MapList::consoleWriteAccess() != 0,
                                  int, std::string, std::string, int, 0, kMapListConsoleTag)

// Handed to the console registry by ModuleGame::init, the way the original registers its own list.
ConsoleObjectList& mapListConsoleObjects()
{
    return consoleBindingList(kMapListConsoleTag);
}

} // namespace hfe
} // namespace dice
