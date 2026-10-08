// Ordered map rotation; validates map and mode before accepting an entry.
#ifndef GAME_MAPLIST_H     // [INFERRED] guard style
#define GAME_MAPLIST_H

#include <deque>
#include <string>

namespace dice {
namespace hfe {

class MapList
{
public:
    // Starts with no current map (-1). instance owns a function-local static; the
    // separate mapList pointer is initialized from it before main.
    MapList();
    static MapList* instance();

    bool load(bool);
    // Returns false without a configuration path or stream, otherwise whether the complete script was written.
    bool save() const;
    // Returns false when modification is disallowed.
    bool clear(bool);

    int  getMapCount() const;                              //   deque size
    int  getCurrentMap() const;                            //   returns [this+4]
    void setCurrentMap(int, bool);                         //   see the override note above
    bool validateIndex(int) const;                         //   0 <= idx < size()
    int  getIndexFromName(const std::string&) const;

    // Map rotation entry. mapSize is a 16-bit value; the trailing insertion flag is
    // not stored as a member. STL fields make the native size toolchain-dependent.
    struct Entry
    {
        std::string name;                                  // +0x00
        std::string gameMode;                              // +0x04
        short       mapSize;                               // +0x08 (word; see note)
    };

    const Entry* getEntry(int) const;
    // Returns false on invalid input or when modification is disallowed.
    bool insertEntry(int, const std::string&, const std::string&, int, bool);
    bool appendEntry(const std::string&, const std::string&, int, bool);
    // Removes the entry at idx, shifting the current selection down when it pointed at or past
    // the removed entry. Returns false on an out-of-range index or when modification is disallowed.
    bool removeEntry(int idx, bool modifiableOverride = false);
    void moveUp(int);
    void moveDown(int);
    bool checkIfValidEntry(const std::string&, const std::string&);            //   NOT const

    bool isModifiable() const;
    bool isDirty() const;                                  //   returns [this+0x30]
    void setDirty(bool);
    void internalSetDirty();                               //   [0x30]=1, [0x31]=0
    bool isCurrentEntryPlasma() const;

    // Console binding access. See the note above - write access returns NULL when not modifiable.
    static MapList* consoleReadAccess();
    static MapList* consoleWriteAccess();

    // Network serialisation
    unsigned char* generateDataBlock() const;              //   IS const; 652 B - returns the new[] buffer
// Lazily serializes the current list as count, current index, then each name, mode, and map size.
// Counts, indices, string lengths, and sizes use 16 bits.
    const unsigned char* getDataBlock() const;             // the +0x38 buffer
    int  getDataBlockSize() const;                         // the +0x34 size
    // Reads the map-list payload using embedded counts. The size argument is ignored, so malformed data can overread.
    void updateFromDataBlock(const unsigned char*, unsigned int);
    void assertDataBlockValid() const;
    // Formats the rotation as one line per entry. `bare` selects the command form the console
    // reports; the ELF executor ConsoleClass761 (0xa91d4b) calls it with false for `maplist.list`.
    // false -> `index: "name" gameMode [mapSize]` (size omitted at zero), true -> `name gameMode size`.
    std::string formatListForConsole(bool bare) const;

// [CHOSEN] Public storage supports console bindings. Offset comments describe the ELF layout only; STL layouts vary.
public:
    // Map rotation filename, set by +mapList or defaulted to the mod's settings/maplist.con.
    /* 0x00 */ std::string m_configFile;
    /* 0x04 */ int m_currentMap;           // ctor -1; getCurrentMap returns it
    /* 0x08 */ std::deque<Entry> m_entries;// ELF: deque guts +0x08..+0x2F, element 12 B
    /* 0x30 */ bool m_dirty;               // isDirty/setDirty/internalSetDirty
    /* 0x31 */ mutable bool m_dataBlockValid;      // internalSetDirty clears it; assertDataBlockValid sets it
    /* 0x34 */ mutable int  m_dataBlockSize;       // generateDataBlock's byte count
    /* 0x38 */ mutable unsigned char* m_dataBlock; // generateDataBlock's new[] buffer (mutable: the
                                           //   const getters regenerate through assertDataBlockValid)
    /* 0x3C */ bool m_modifiableOverride;  // the flag setCurrentMap/appendEntry/insertEntry's
                                           // trailing bool temporarily raises (see note above)
};

extern MapList* mapList;      // ds:0x8AAAA00

} // namespace hfe
} // namespace dice

#endif
