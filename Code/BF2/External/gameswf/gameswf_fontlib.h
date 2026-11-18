// gameswf_fontlib.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Internal interfaces to fontlib.


#ifndef GAMESWF_FONTLIB_H
#define GAMESWF_FONTLIB_H


#include "base/container.h"
#include "gameswf_types.h"
class tu_file;


namespace gameswf
{
	struct movie_def_impl;

	struct texture_glyph;
	namespace fontlib
	{
		// For adding fonts.
		void	add_font(font* f);

		// For drawing a textured glyph w/ current render transforms.
		void	draw_glyph(const matrix& m, const texture_glyph& g, rgba color, int nominal_glyph_height);

		// Return the pixel height of text, such that the
		// texture glyphs are sampled 1-to-1 texels-to-pixels.
		// I.e. the height of the glyph box, in texels.
		float	get_texture_glyph_max_height(const font* f);

		// Builds cached glyph textures from shape info.
		void	generate_font_bitmaps(const array<font*>& fonts, movie_definition_sub* owner);
		
		// Save cached font data, including glyph textures, to a
		// stream.
		void	output_cached_data(
			tu_file* out,
			const array<font*>& fonts,
			movie_definition_sub* owner,
			const cache_options& options);
		
		// Load a stream containing previously-saved cachded font
		// data, including glyph texture info.
		void	input_cached_data(tu_file* in, const array<font*>& fonts, movie_definition_sub* owner);

		// [CHOSEN, not recovered] HiDPI menu text (SwiffPlayerAbi/HiDpiGlyphs.h): remember an
		// ExportAssets font under its export name for the renderer's .dif atlas rebuild.
		void	hidpi_register_exported_font(const char* export_name, font* f);
		// Every DefineFont2 with outlines, for fonts without an export name.
		void	hidpi_register_font(font* f);

	}	// end namespace fontlib
}	// end namespace gameswf



#endif // GAMESWF_FONTLIB_H

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
