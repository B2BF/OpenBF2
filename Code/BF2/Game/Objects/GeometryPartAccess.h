// Path: [CHOSEN, not recovered] shared narrow seams for the still-opaque geometry interface.
// Soldier ELF 0x81B5D60/client PE 0x61F630 and Armor ELF 0x8218F30/client PE 0x685770
// independently prove these slots. RendDX9 BundledMesh table 0x1021B350 supplies the callees.
// No positional C++ interface, concrete renderer dependency or geometry layout is invented.
#ifndef BF2_GAME_OBJECTS_GEOMETRYPARTACCESS_H
#define BF2_GAME_OBJECTS_GEOMETRYPARTACCESS_H

#include "../../Math/Mat4.h"
#include <cstring>
#include <string>
#if defined(_MSC_VER) && defined(_M_IX86)
#define BF2_GEOMETRY_PART_CALL __thiscall
#else
#define BF2_GEOMETRY_PART_CALL
#endif

namespace dice { namespace hfe { namespace world { namespace geometry_part {

template<class Function> inline Function slot(void* geometry, unsigned int pe, unsigned int elf)
{
#if defined(_MSC_VER)
    const unsigned int offset = pe;
#else
    const unsigned int offset = elf;
#endif
    // The evidence is a 32-bit byte offset. Convert it to a logical ordinal before
    // indexing a native pointer table; sizeof(void*) would halve every x64 ordinal.
    return reinterpret_cast<Function>((*static_cast<void***>(geometry))[offset / 4]);
}

inline int count(void* geometry)
{
    typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x5C, 0x60)(geometry); // Bundled 0x10103DA0
}

inline void setTransform(void* geometry, const Mat4* transform, int part)
{
    typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, const Mat4*, int);
    // Unlike the adjacent palette slot, the individual-part slot shifts by eight bytes.
    // keeps the pointer, not a copy; callers supply stable matrix storage.
    slot<Function>(geometry, 0x4C, 0x54)(geometry, transform, part);
}

inline int getLod(void* geometry)
{
    typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x7C, 0x80)(geometry);
}

inline int getLodCount(void* geometry)
{
    typedef int (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x80, 0x84)(geometry);
}

inline void setLod(void* geometry, int lod)
{
    typedef void (BF2_GEOMETRY_PART_CALL* Function)(void*, int);
    slot<Function>(geometry, 0x78, 0x7C)(geometry, lod);
}

inline void* getTemplate(void* geometry)
{
    typedef void* (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(geometry, 0x38, 0x3C)(geometry);
}

inline bool templateNameEquals(void* objectTemplate, const char* name)
{
    // MeshTemplate::getName (+0x18) returns the renderer's std::string: the client and its
    // plugins share one STL (issue #7).
    typedef const std::string& (BF2_GEOMETRY_PART_CALL* Function)(void*);
    return slot<Function>(objectTemplate, 0x18, 0x18)(objectTemplate) == name;
}

} } } }
#undef BF2_GEOMETRY_PART_CALL
#endif
