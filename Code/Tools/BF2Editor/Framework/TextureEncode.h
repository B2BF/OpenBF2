// Encodes the game's terrain textures the way the retail editor's texture tool (nvdxt) wrote them: DXT1 colour with or
// without the full mip chain, and 16-bit R5G6B5 without mips, each with nvdxt's DDS header (the linear size set). The
// box filter halves an image with 2 x 2 means, as nvdxt's -Box prescale and mip chain do. The DXT1 blocks come from an
// encoder of our own (endpoints on each block's principal axis, refined by least squares), so the bytes differ from
// nvdxt's while the format is the same. Engine-free. Editor-only code.
#ifndef BF2EDITOR_FRAMEWORK_TEXTUREENCODE_H
#define BF2EDITOR_FRAMEWORK_TEXTUREENCODE_H

#include "Framework/EditorConfig.h"
#include "Framework/DdsImage.h"

#include <string>

namespace bf2editor
{

// Half the width and height (at least 1), each texel the rounded mean of the 2 x 2 texels it covers.
DdsImage halveBox(const DdsImage& image);

// A DXT1 DDS of `image` (alpha ignored; sides a power of two); with `mipmaps`, every level down to 1 x 1, each halved
// from the one before by the box filter.
std::string encodeDxt1(const DdsImage& image, bool mipmaps);

// A 16-bit R5G6B5 DDS of `image` without mips, each channel rounded to its bits.
std::string encodeR5G6B5(const DdsImage& image);

} // namespace bf2editor

#endif // BF2EDITOR_FRAMEWORK_TEXTUREENCODE_H
