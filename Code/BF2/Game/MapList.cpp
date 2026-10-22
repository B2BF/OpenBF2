// Map-list validation, mutation, persistence, and serialization. Source path is attested.

#include "MapList.h"

#include "IGame.h"                     // isModifiable dispatches on the game global
#include "IModInfo.h"                  // checkIfValidEntry validates against ModInfo
#include "../Core/Ids.h"               // IID_IGameServer
#include "../Core/ClassManager.h"      // getSingleton("ModInfo")
#include "../Core/StringUtil.h"        // compareIgnoreCase
#include "../Core/Debug.h"
#include "../IO/Console/Console.h"      // load() runs maplist.con through io::mainConsole
#include "../IO/File/FileManager.h"      // save() writes maplist.con through io::fileManager
#include "../IO/Stream/IStream.h"
#include <sstream>

#include <string.h>                    // memcpy - the data block's write_raw

namespace dice {
namespace hfe {

extern ClassManager* classManager;     // Shared class registry.
                                       // A fresh rotation has no current map; its index starts at -1.
MapList::MapList()
{
    m_currentMap = -1;          // : [this+4] = -1
    m_dirty = false;            // +0x30
    m_dataBlockValid = false;   // +0x31
    m_dataBlockSize = 0;        // +0x34
    m_dataBlock = 0;            // +0x38
    m_modifiableOverride = false;   // +0x3C
    // m_configFile / m_entries: default-constructed (the ELF default-constructs the +0x00 string and
    // _M_initialize_map's the deque - same observable state).
}

// instance - ELF 0x80607A0. The guarded-function-local-static pattern (guard byte 0x8AAAA08,
// object 0x8AAAA20, __cxa_atexit) = a Meyers singleton, transcribed as one.
MapList* MapList::instance()
{
    static MapList s;
    return &s;
}

// Publish the map-list singleton before command-line initialization uses it.
MapList* mapList = MapList::instance();

// Writes maplist.append commands to the configured file and releases its stream.
// Returns false without a path/stream or if the write count differs from text size.
// Map size is formatted as a short.
bool MapList::save() const
{
    if (m_configFile.empty())
        return false;
    io::IStream* stream = io::fileManager->open(m_configFile, (io::FileMode)0, (io::FileAccess)2, 0);
    if (!stream)
        return false;
    std::stringstream script(std::ios::in | std::ios::out);
    for (std::deque<Entry>::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
        script << "maplist.append \"" << it->name << "\" \"" << it->gameMode << "\" " << it->mapSize
               << std::endl;
    std::string text = script.str();
    const unsigned int written = stream->write(text.data(), (unsigned int)text.size());
    stream->release();
    return written == text.size();
}

// trivial accessors - semantics proven in MapList.h's notes
int  MapList::getMapCount() const  { return (int)m_entries.size(); }      //  (deque size)
int  MapList::getCurrentMap() const { return m_currentMap; }
bool MapList::isDirty() const      { return m_dirty; }
void MapList::setDirty(bool v)     { m_dirty = v; }
void MapList::internalSetDirty()   { m_dirty = true; m_dataBlockValid = false; }

// Returns the first exact, case-sensitive name match, or -1. Game mode is ignored.
int MapList::getIndexFromName(const std::string& name) const
{
    int index = 0;
    for (std::deque<Entry>::const_iterator it = m_entries.begin(); it != m_entries.end();
         ++it, ++index)
        if (name == it->name)
            return index;
    return -1;
}

// Wire block: u16 entry count, u16 current index, then length-prefixed name and
// game-mode bytes plus u16 map size per entry. Values use raw host order.
// Mutations invalidate the cached buffer; the next read rebuilds it at exact size.
namespace {
inline void write_raw(unsigned char*& p, unsigned short v)
{
    *reinterpret_cast<unsigned short*>(p) = v;
    p += 2;
}
inline void write_raw(unsigned char*& p, const void* src, unsigned int n)
{
    memcpy(p, src, n);
    p += n;
}
inline void write_raw(unsigned char*& p, const std::string& s)
{
    const unsigned short length = static_cast<unsigned short>(s.length());
    write_raw(p, length);
    write_raw(p, s.data(), length);
}

// Reads host-order scalars and length-prefixed strings without bounds checks; advances the input pointer.
inline void read_raw(const unsigned char*& p, unsigned short& v)
{
    v = *reinterpret_cast<const unsigned short*>(p);
    p += 2;
}
inline void read_raw(const unsigned char*& p, short& v)
{
    v = *reinterpret_cast<const short*>(p);
    p += 2;
}
inline void read_raw(const unsigned char*& p, std::string& s)
{
    unsigned short length = 0;
    read_raw(p, length);
    s.assign(reinterpret_cast<const char*>(p), length);
    p += length;
}
} // namespace

unsigned char* MapList::generateDataBlock() const
{
    unsigned int size = 4;                                                // count + current map
    for (std::deque<Entry>::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
        size += static_cast<unsigned int>(it->name.length() + it->gameMode.length()) + 4 + 2;
    m_dataBlockSize = static_cast<int>(size);

    unsigned char* block = new unsigned char[size];
    unsigned char* p = block;
    write_raw(p, static_cast<unsigned short>(m_entries.size()));
    write_raw(p, static_cast<unsigned short>(m_currentMap));
    for (std::deque<Entry>::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
    {
        write_raw(p, it->name);
        write_raw(p, it->gameMode);
        write_raw(p, static_cast<unsigned short>(it->mapSize));
    }
    return block;
}

// Replaces map entries from the serialized count and current-index header. Ignores
// size and performs no buffer-bound validation; callers must supply a complete block.
// Does not dirty or regenerate the outgoing cache.
void MapList::updateFromDataBlock(const unsigned char* data, unsigned int /*size - unused, as shipped*/)
{
    m_entries.clear();

    const unsigned char* p = data;
    unsigned short count      = 0;
    unsigned short currentMap = 0;
    read_raw(p, count);
    read_raw(p, currentMap);

    for (unsigned short i = 0; i < count; ++i)
    {
        Entry entry;
        read_raw(p, entry.name);
        read_raw(p, entry.gameMode);
        read_raw(p, entry.mapSize);
        m_entries.push_back(entry);
    }

    m_currentMap = currentMap;
}

// : rebuild when stale.
void MapList::assertDataBlockValid() const
{
    if (!m_dataBlockValid)
    {
        if (m_dataBlock)
            delete[] m_dataBlock;
        m_dataBlock = generateDataBlock();
        m_dataBlockValid = true;
    }
}

int MapList::getDataBlockSize() const
{
    assertDataBlockValid();
    return m_dataBlockSize;
}

const unsigned char* MapList::getDataBlock() const
{
    assertDataBlockValid();
    return m_dataBlock;
}

// validateIndex - ELF 0x8062CF0, 115 bytes. The long deque-size arithmetic in the body IS
// deque::size(); the function is a bounds check.
bool MapList::validateIndex(int idx) const
{
    return idx >= 0 && idx < (int)m_entries.size();
}

// isModifiable - ELF 0x8060C70, 72 bytes.  (see MapList.h's header block): a server
// may always edit the list; otherwise only when the +0x3C override is raised.
bool MapList::isModifiable() const
{
    if (game && game->queryInterface(IID_IGameServer))
        return true;
    return m_modifiableOverride;
}

// setCurrentMap - ELF 0x80615B0, 90 bytes.  (MapList.h): the bool temporarily raises
// the +0x3C override so isModifiable passes, and restores it on every path.
void MapList::setCurrentMap(int idx, bool modifiableOverride)
{
    bool saved = m_modifiableOverride;
    if (modifiableOverride)
        m_modifiableOverride = modifiableOverride;
    if (!isModifiable())
    {
        m_modifiableOverride = saved;
        return;
    }
    m_currentMap = idx;             // [this+4]
    internalSetDirty();
    m_modifiableOverride = saved;
}

// Returns a borrowed rotation entry or null for an invalid index.
const MapList::Entry* MapList::getEntry(int idx) const
{
    if (!validateIndex(idx))
        return 0;
    return &m_entries[idx];
}

// maplist.list formatting, recovered from MapList::formatListForConsole (ELF 0x4199b0), the
// formatter the list executor calls. The false arm emits `<index>: "<name>" <gameMode> <mapSize>`
// and omits the size when it is zero; the true arm emits the bare `<name> <gameMode> <mapSize>` line.
std::string MapList::formatListForConsole(bool bare) const
{
    std::ostringstream out;
    int index = 0;
    for (std::deque<Entry>::const_iterator it = m_entries.begin(); it != m_entries.end(); ++it)
    {
        if (bare)
        {
            out << it->name << " " << it->gameMode << " " << it->mapSize << "\n";
        }
        else
        {
            out << index << ": \"" << it->name << "\" " << it->gameMode;
            if (it->mapSize)
                out << " " << it->mapSize;
            out << "\n";
        }
        ++index;
    }
    return out.str();
}

// Requires the map in the current mod. sp1 skips mode validation; other modes
// must be supported by the map. For bf2/xpack, a nonempty list requires the first
// entry's mode. Validation failures log and return false.
bool MapList::checkIfValidEntry(const std::string& mapName, const std::string& gameMode)
{
    IModInfo* modInfo = (IModInfo*)classManager->getSingleton("ModInfo");    // vtable +0x4C
    if (!modInfo)
        return false;

    const IModInfo::Map* map = modInfo->findMapFromCurrentMod(mapName);      // vtable +0x24
    if (!map)
    {
        Debug(DEBUGTYPE_1, "Game/MapList.cpp", 277, "")
            << "could not add to maplist. map not found:" << mapName << " gameMode:" << gameMode;
        return false;
    }

    bool foundGameMode = false;
    if (compareIgnoreCase(gameMode, "sp1") != 0)
    {
        // The 4-dword stride in the raw loop is scaled pointer arithmetic over the 16-byte
        // GameMode elements; the compare hits each element's name at +0x00.
        for (std::vector<IModInfo::GameMode>::const_iterator it = map->gameModes.begin();
             it != map->gameModes.end(); ++it)
        {
            if (compareIgnoreCase(it->name, gameMode) == 0)
            {
                foundGameMode = true;
                break;
            }
        }

        // The same-mode rule applies only to the two stock mods.
        std::string modPath = modInfo->getCurrentModPath();                  // vtable +0x20
        bool stockMod = (compareIgnoreCase(modPath, "mods/bf2") == 0)
                     || (compareIgnoreCase(modPath, "mods/xpack") == 0);
        if (stockMod && !m_entries.empty())
        {
            if (compareIgnoreCase(m_entries.front().gameMode, gameMode) != 0)
            {
                Debug(DEBUGTYPE_1, "Game/MapList.cpp", 306, "")
                    << "could not add to maplist. game mode must be the same:" << mapName
                    << " gameMode:" << gameMode;
                return false;
            }
        }
    }
    else
    {
        foundGameMode = true;
    }

    if (!foundGameMode)
    {
        Debug(DEBUGTYPE_1, "Game/MapList.cpp", 317, "")
            << "could not add to maplist. gameMode not found:" << mapName
            << " gameMode:" << gameMode;
        return false;
    }
    return true;
}

// Clamps map size to 0–64 and validates the entry before checking modification permission.
bool MapList::appendEntry(const std::string& name, const std::string& gameMode,
                          int mapSize, bool modifiableOverride)
{
    short clamped = 0;
    if (mapSize >= 0)
    {
        clamped = (short)mapSize;
        if (mapSize >= 65)
            clamped = 64;
    }

    if (!checkIfValidEntry(name, gameMode))
        return false;

    bool saved = m_modifiableOverride;
    if (modifiableOverride)
        m_modifiableOverride = modifiableOverride;
    if (!isModifiable())
    {
        m_modifiableOverride = saved;
        return false;
    }

    Entry e;
    e.name = name;
    e.gameMode = gameMode;
    e.mapSize = clamped;
    m_entries.push_back(e);

    internalSetDirty();
    m_modifiableOverride = saved;
    return true;
}

// Inserts before an existing index, preserving the current selection.
// Cannot insert into an empty list; use appendEntry to seed it.
bool MapList::insertEntry(int idx, const std::string& name, const std::string& gameMode,
                          int mapSize, bool modifiableOverride)
{
    short clamped = 0;
    if (mapSize >= 0)
    {
        clamped = (short)mapSize;
        if (mapSize >= 65)
            clamped = 64;
    }

    if (!checkIfValidEntry(name, gameMode))
        return false;

    bool saved = m_modifiableOverride;
    if (modifiableOverride)
        m_modifiableOverride = modifiableOverride;

    if (!validateIndex(idx))
    {
        m_modifiableOverride = saved;
        return false;
    }
    if (!isModifiable())
    {
        m_modifiableOverride = saved;
        return false;
    }

    Entry e;
    e.name = name;
    e.gameMode = gameMode;
    e.mapSize = clamped;
    m_entries.insert(m_entries.begin() + idx, e);

    if (idx <= m_currentMap)
        ++m_currentMap;             // 0x8062xxx: the selection follows its entry

    internalSetDirty();
    m_modifiableOverride = saved;
    return true;
}

// Removes the entry at idx. Follows insertEntry's guard order (range, then permission) and
// keeps the current selection pointing at the same entry. Recovered from the dedicated PE
// executor's callee (0x441750), whose trailing bool is the temporary modification override.
bool MapList::removeEntry(int idx, bool modifiableOverride)
{
    const bool saved = m_modifiableOverride;
    if (modifiableOverride)
        m_modifiableOverride = modifiableOverride;

    if (!validateIndex(idx) || !isModifiable())
    {
        m_modifiableOverride = saved;
        return false;
    }

    internalSetDirty();
    m_entries.erase(m_entries.begin() + idx);
    if (idx <= m_currentMap)
        --m_currentMap;

    m_modifiableOverride = saved;
    return true;
}

// Console reads always resolve the map list. Writes require isModifiable; returning
// null hides write bindings. The write receiver requires the initialized global list.
MapList* MapList::consoleReadAccess()
{
    return mapList;                                       // ds:0x8AAAA00
}

MapList* MapList::consoleWriteAccess()
{
    if (!mapList->isModifiable())
        return 0;
    return mapList;
}

// Temporarily enables modification only when requested, then restores the prior permission on every exit.
bool MapList::clear(bool modifiableOverride)
{
    const bool saved = m_modifiableOverride;              // +0x3C -> var_5
    if (modifiableOverride)
        m_modifiableOverride = modifiableOverride;        // only when true

    if (!isModifiable())
    {
        m_modifiableOverride = saved;
        return false;                                     //  xor eax,eax
    }

    m_currentMap = -1;                                    // NOT 0
    m_entries.clear();
    internalSetDirty();

    m_modifiableOverride = saved;
    return true;                                          //  mov eax,1
}

// Loads the configured map-list console script and returns its result. Marks the
// list dirty before checking modifiability; a refusal still invalidates cached data.
// Restores relative-path mode and the modifiable override after execution.
bool MapList::load(bool modifiableOverride)
{
    if (m_configFile.empty())                             // the only gate
        return false;

    internalSetDirty();                                   // before the permission test

    const bool saved = m_modifiableOverride;              // +0x3C
    if (modifiableOverride)
        m_modifiableOverride = modifiableOverride;
    if (isModifiable())
        clear(false);                                     // false, deliberately

    const bool useRelative = io::mainConsole->getUseRelativePaths();
    io::mainConsole->setUseRelativePaths(false);

    const bool ok = io::mainConsole->run(m_configFile, "", "", "", "", "", "", "", "",
                                         io::Console::ignoredString_);   // vt +0x28

    io::mainConsole->setUseRelativePaths(useRelative);
    m_modifiableOverride = saved;
    return ok;
}

} // namespace hfe
} // namespace dice
