#ifndef BF2_HIDPIFONTTEXTURE_H
#define BF2_HIDPIFONTTEXTURE_H

#include <string>

namespace dice { namespace hfe { namespace geom {

// 1 transfers an atlas reference. 0 requests a retry when more SWF outlines arrive.
// -1 keeps the shipped atlas.
int hidpiBuildFontTexture(const std::string& base, const char* exportName, int scale, void** outTexture);
void hidpiAddPending(void* owner, void (*retry)(void*));
void hidpiRemovePending(void* owner);

} } }

#endif
