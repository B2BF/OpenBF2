// The comparator the decal owners' std containers need beyond the defaults. Retail's decal
// containers are VC7.1 std::vector / std::map instances; the rebuilt renderer uses this toolset's
// std containers instead (issue #7), so only the template map's case-insensitive order is kept here.

#ifndef BF2_GEOM_DECALSYSTEM_DECALCONTAINERS_H
#define BF2_GEOM_DECALSYSTEM_DECALCONTAINERS_H

#ifdef BF2_IMAGE_RENDDX9

#include <string>

#include <map>
#include <string.h>
#include <vector>

namespace dice {
namespace hfe {
namespace geom {

// The template map's comparator: 0x10036390 (find, comdat-shared with GeometryTemplateManager's
// registry) and 0x10126CE0 (operator[]) both call MSVCR71 `_stricmp(a.c_str(), b.c_str())`.
// std::map keeps the first-inserted spelling of a key, as the shipped map does.
struct DecalTemplateNameLess
{
    bool operator()(const std::string& a, const std::string& b) const
    {
        return ::_stricmp(a.c_str(), b.c_str()) < 0;
    }
};

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9

#endif // BF2_GEOM_DECALSYSTEM_DECALCONTAINERS_H
