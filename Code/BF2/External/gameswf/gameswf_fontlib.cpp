// gameswf_fontlib.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// A module to take care of all of gameswf's loaded fonts.


#include <cmath>
#include "base/container.h"
#include "base/tu_file.h"
#include "gameswf.h"
#include "gameswf_font.h"
#include "gameswf_impl.h"
#include "gameswf_log.h"
#include "gameswf_render.h"
#include "gameswf_shape.h"
#include "../../Platform/X87Compat.h"
#include "gameswf_styles.h"
#include "gameswf_tesselate.h"
#include "base/smart_ptr.h"
#include "../../../SwiffPlayerAbi/HiDpiGlyphs.h"	// [CHOSEN, not recovered] HiDPI menu text
#include <ctype.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "gameswf_render.h"


namespace gameswf
{
namespace fontlib
{
	array< smart_ptr<font> >	s_fonts;

	// Size (in TWIPS) of the box that the glyph should
	// stay within.
	static float	s_rendering_box = 1536.0f;	// this *should* be 1024, but some glyphs in some fonts exceed it!

	// The nominal size of the final antialiased glyphs stored in
	// the texture.  This parameter controls how large the very
	// largest glyphs will be in the texture; most glyphs will be
	// considerably smaller.  This is also the parameter that
	// controls the tradeoff between texture RAM usage and
	// sharpness of large text.
	static int	s_glyph_nominal_size =
// You can override the default rendered glyph size in
// compatibility_include.h, to trade off memory vs. niceness of large
// glyphs.  You can also override this at run-time via
// gameswf::fontlib::set_nominal_glyph_pixel_size()
#ifndef GAMESWF_FONT_NOMINAL_GLYPH_SIZE_DEFAULT
	96
#else
	GAMESWF_FONT_NOMINAL_GLYPH_SIZE_DEFAULT
#endif
	;

	static const int	OVERSAMPLE_BITS = 2;
	static const int	OVERSAMPLE_FACTOR = (1 << OVERSAMPLE_BITS);

	// The dimensions of the textures that the glyphs get packed into.
	static const int	GLYPH_CACHE_TEXTURE_SIZE = 256;

	// How much space to leave around the individual glyph image.
	// This should be at least 1.  The bigger it is, the smoother
	// the boundaries of minified text will be, but the more
	// texture space is wasted.
	const int PAD_PIXELS = 3;


	// The raw non-antialiased render size for glyphs.
	static int	s_glyph_render_size = s_glyph_nominal_size << OVERSAMPLE_BITS;

	
	// SwiffPlayer.dll 0x100A47F0.
	void	set_nominal_glyph_pixel_size(int pixel_size)
	{
		static const int	MIN_SIZE = 4;
		static const int	MAX_SIZE = GLYPH_CACHE_TEXTURE_SIZE / 2;

		if (pixel_size < MIN_SIZE)
		{
			// Retail supplies only 4 for the two conversions on this branch.
			// Preserve the missing argument; the callback must be disabled to
			// exercise this clamp without the shipped formatter's unchecked read.
			void (*report)(const char*, ...) = log_error;
			report("set_nominal_glyph_pixel_size(%d) too small, clamping to %d\n", MIN_SIZE);
			pixel_size = MIN_SIZE;
		}
		else if (pixel_size > MAX_SIZE)
		{
			log_error("set_nominal_glyph_pixel_size(%d) too large, clamping to %d\n",
				  pixel_size,
				  MAX_SIZE);
			pixel_size = MAX_SIZE;
		}

		s_glyph_nominal_size = pixel_size;
		s_glyph_render_size = s_glyph_nominal_size << OVERSAMPLE_BITS;
	}


	//
	// State for the glyph packer.
	//

	static Uint8*	s_render_buffer = NULL;
	static matrix	s_render_matrix;

	static Uint8*	s_current_cache_image = NULL;

	// for setting the bitmap_info after they're packed.
	struct pending_glyph_info
	{
		font*	m_source_font;
		int	m_glyph_index;
		texture_glyph	m_texture_glyph;

		// SwiffPlayer.dll 0x100A64C0: source font, index, then texture glyph.
		pending_glyph_info& operator=(const pending_glyph_info&) = default;

		// SwiffPlayer.dll 0x100A4F70; implicit member destruction at 0x100A52B0.
		pending_glyph_info()
			:
			m_source_font(NULL),
			m_glyph_index(-1)
		{
		}

		pending_glyph_info(font* f, int gi, const texture_glyph& tg)
			:
			m_source_font(f),
			m_glyph_index(gi),
			m_texture_glyph(tg)
		{
		}
	};
	static array< pending_glyph_info >	s_pending_glyphs;


	// Integer-bounded 2D rectangle.
	struct recti
	{
		int	m_x_min, m_x_max, m_y_min, m_y_max;

		// SwiffPlayer.dll 0x100A4870: four ordered integer stores.
		recti(int x0 = 0, int x1 = 0, int y0 = 0, int y1 = 0)
			:
			m_x_min(x0),
			m_x_max(x1),
			m_y_min(y0),
			m_y_max(y1)
		{
		}

		bool	is_valid() const
		{
			return m_x_min <= m_x_max
				&& m_y_min <= m_y_max;
		}

		bool	intersects(const recti& r) const
		// Return true if r touches *this.
		{
			if (m_x_min >= r.m_x_max
			    || m_x_max <= r.m_x_min
			    || m_y_min >= r.m_y_max
			    || m_y_max <= r.m_y_min)
			{
				// disjoint.
				return false;
			}
			return true;
		}

		bool	contains(int x, int y) const
		// Return true if (x,y) is inside *this.
		{
			return x >= m_x_min
				&& x < m_x_max
				&& y >= m_y_min
				&& y < m_y_max;
		}
	};
	// Rects already on the texture.
	static array<recti>	s_covered_rects;

	// 2d integer point.
	struct pointi
	{
		int	m_x, m_y;

		// SwiffPlayer.dll 0x100A48F0.
		pointi(int x = 0, int y = 0)
			:
			m_x(x),
			m_y(y)
		{
		}

		// SwiffPlayer.dll 0x100A4910.
		bool	operator<(const pointi& p) const
		// For sorting anchor points.
		{
			return imin(m_x, m_y) < imin(p.m_x, p.m_y);
		}
	};
	// Candidates for upper-left corner of a new rectangle.  Use
	// lower-left and upper-right of previously placed rects.
	static array<pointi>	s_anchor_points;


	static bool	s_saving = false;
	static bool	s_save_dummy_bitmaps = false;
	static tu_file*	s_file = NULL;


	// SwiffPlayer.dll 0x100A66F0.
	static void	ensure_cache_image_available()
	{
		if (s_pending_glyphs.size() == 0)
		{
			// Set up a cache.
			if (s_current_cache_image == NULL)
			{
				s_current_cache_image = new Uint8[GLYPH_CACHE_TEXTURE_SIZE * GLYPH_CACHE_TEXTURE_SIZE];
			}
			memset(s_current_cache_image, 0, GLYPH_CACHE_TEXTURE_SIZE * GLYPH_CACHE_TEXTURE_SIZE);

			// Initialize the coverage data.
			s_covered_rects.resize(0);
			s_anchor_points.resize(0);
			s_anchor_points.push_back(pointi(0, 0));	// seed w/ upper-left of texture.
		}
	}


	// SwiffPlayer.dll 0x100A6BE0.
	void	finish_current_texture(movie_definition_sub* owner)
	{
		if (s_pending_glyphs.size() == 0)
		{
			return;
		}

#if 0
		//xxxxxx debug hack -- dump image data to a file
		static int	s_seq = 0;
		char buffer[100];
		sprintf(buffer, "dump%02d.ppm", s_seq);
		s_seq++;
		FILE*	fp = fopen(buffer, "wb");
		if (fp)
		{
			fprintf(fp, "P6\n%d %d\n255\n", GLYPH_CACHE_TEXTURE_SIZE, GLYPH_CACHE_TEXTURE_SIZE);
			for (int i = 0; i < GLYPH_CACHE_TEXTURE_SIZE * GLYPH_CACHE_TEXTURE_SIZE; i++)
			{
				fputc(s_current_cache_image[i], fp);
				fputc(s_current_cache_image[i], fp);
				fputc(s_current_cache_image[i], fp);
			}
			fclose(fp);
		}
		//xxxxxx
#endif // 0

		if (s_saving)		// HACK!!!
		{
			if (s_save_dummy_bitmaps)
			{
				// Save a mini placeholder bitmap.
				s_file->write_le16(1);
				s_file->write_le16(1);
				s_file->write_byte(0);
			}
			else
			{
				int w = GLYPH_CACHE_TEXTURE_SIZE;
				int h = GLYPH_CACHE_TEXTURE_SIZE;

				// save bitmap size
				s_file->write_le16(w);
				s_file->write_le16(h);

				// save bitmap contents
				s_file->write_bytes(s_current_cache_image, w*h);
			}
		}

		smart_ptr<bitmap_info>	bi;
		if (owner->get_create_bitmaps() == DO_NOT_LOAD_BITMAPS)
		{
			bi = render::create_bitmap_info_empty();
		}
		else
		{
			bi = render::create_bitmap_info_alpha(
				GLYPH_CACHE_TEXTURE_SIZE,
				GLYPH_CACHE_TEXTURE_SIZE,
				s_current_cache_image);
		}
		owner->add_bitmap_info(bi.get_ptr());

		// Push finished glyphs into their respective fonts.
		for (int i = 0, n = s_pending_glyphs.size(); i < n; i++)
		{
			pending_glyph_info*	pgi = &s_pending_glyphs[i];
			assert(pgi->m_glyph_index != -1);
			assert(pgi->m_source_font != NULL);

			pgi->m_texture_glyph.set_bitmap_info(bi.get_ptr());
			pgi->m_source_font->add_texture_glyph(pgi->m_glyph_index, pgi->m_texture_glyph);
			//s_pending_glyphs[i]->set_bitmap_info(bi.get_ptr());
		}
		s_pending_glyphs.clear();
	}


	// SwiffPlayer.dll 0x100A5020: reject overflow, then strict rectangle overlaps.
	bool	is_rect_available(const recti& r)
	// Return true if the given rect can be packed into the
	// currently active texture.
	{
		assert(r.is_valid());
		assert(r.m_x_min >= 0);
		assert(r.m_y_min >= 0);

		if (r.m_x_max > GLYPH_CACHE_TEXTURE_SIZE
		    || r.m_y_max > GLYPH_CACHE_TEXTURE_SIZE)
		{
			// Rect overflows the texture bounds.
			return false;
		}

		// Check against existing rects.
		for (int i = 0, n = s_covered_rects.size(); i < n; i++)
		{
			if (r.intersects(s_covered_rects[i]))
			{
				return false;
			}
		}

		// Spot appears to be open.
		return true;
	}


	// SwiffPlayer.dll 0x100A6800.
	void	add_cover_rect(const recti& r)
	// Add the given rect to our list.  Eliminate any anchor
	// points that are disqualified by this new rect.
	{
		s_covered_rects.push_back(r);

		for (int i = 0; i < s_anchor_points.size(); i++)
		{
			const pointi&	p = s_anchor_points[i];
			if (r.contains(p.m_x, p.m_y))
			{
				// Eliminate this point from consideration.
				s_anchor_points.remove(i);
				i--;
			}
		}
	}


	// SwiffPlayer.dll 0x100A68C0.
	void	add_anchor_point(const pointi& p)
	// Add point to our list of anchors.  Keep the list sorted.
	{
		// Add it to end, since we expect new points to be
		// relatively greater than existing points.
		s_anchor_points.push_back(p);

		// Insertion sort -- bubble down into correct spot.
		for (int i = s_anchor_points.size() - 2; i >= 0; i--)
		{
			if (s_anchor_points[i + 1] < s_anchor_points[i])
			{
				swap(&(s_anchor_points[i]), &(s_anchor_points[i + 1]));
			}
			else
			{
				// Done bubbling down.
				break;
			}
		}
	}


	// SwiffPlayer.dll 0x100A6950.
	bool	pack_rectangle(int* px, int* py, int width, int height)
	// Find a spot for the rectangle in the current cache image.
	// Return true if there's a spot; false if there's no room.
	{
		// Nice algo, due to JARE:
		//
		// * keep a list of "candidate points"; initialize it with {0,0}
		//
		// * each time we add a rect, add its lower-left and
		// upper-right as candidate points.
		//
		// * search the candidate points only, when looking
		// for a good spot.  If we find a good one, also try
		// scanning left or up as well; sometimes this can
		// close some open space.
		//
		// * when we use a candidate point, remove it from the list.

		// Consider candidate spots.
		for (int i = 0, n = s_anchor_points.size(); i < n; i++)
		{
			const pointi&	p = s_anchor_points[i];
			recti	r(p.m_x, p.m_x + width, p.m_y, p.m_y + height);

			// Is this spot any good?
			if (is_rect_available(r))
			{
				// Good spot.  Scan left to see if we can tighten it up.
				while (r.m_x_min > 0)
				{
					recti	r2(r.m_x_min - 1, r.m_x_min - 1 + width, r.m_y_min, r.m_y_min + height);
					if (is_rect_available(r2))
					{
						// Shift left.
						r = r2;
					}
					else
					{
						// Not clear; stop scanning.
						break;
					}
				}

				// Mark our covered rect; remove newly covered anchors.
				add_cover_rect(r);

				// Found our desired spot.  Add new
				// candidate points to the anchor list.
				add_anchor_point(pointi(r.m_x_min, r.m_y_max));	// lower-left
				add_anchor_point(pointi(r.m_x_max, r.m_y_min));	// upper-right

				*px = r.m_x_min;
				*py = r.m_y_min;

				return true;
			}
		}

		// Couldn't find a good spot.
		return false;
	}


	// This is for keeping track of our rendered glyphs, before
	// packing them into textures and registering with the font.
	struct rendered_glyph_info
	{
		font*	m_source_font;
		int	m_glyph_index;
		image::alpha*	m_image;
		unsigned int	m_image_hash;
		float	m_offset_x;
		float	m_offset_y;

		// SwiffPlayer.dll 0x100A4940.
		rendered_glyph_info()
			:
			m_source_font(0),
			m_glyph_index(0),
			m_image(0),
			m_image_hash(0),
			m_offset_x(0),
			m_offset_y(0)
		{
		}
	};


	// SwiffPlayer.dll 0x100A5140.
	static void	software_trapezoid(
		float y0, float y1,
		float xl0, float xl1,
		float xr0, float xr1)
	// Fill the specified trapezoid in the software output buffer.
	{
		assert(s_render_buffer);

		int	iy0 = bf2x87::toIntTruncate(std::ceil(double(y0)));
		int	iy1 = bf2x87::toIntTruncate(std::ceil(double(y1)));
		float	dy = bf2x87::store(bf2x87::sub(bf2x87::Real(y1), bf2x87::Real(y0)));

		for (int y = iy0; y < iy1; y++)
		{
			if (y < 0) continue;
			if (y >= s_glyph_render_size) return;

			float	f = bf2x87::store(bf2x87::div(
				bf2x87::sub(bf2x87::Real(y), bf2x87::Real(y0)), bf2x87::Real(dy)));
			// Each interpolated edge is stored as double for CRT ceil, not float.
			const double left = bf2x87::storeDouble(bf2x87::add(bf2x87::mul(
				bf2x87::sub(bf2x87::Real(xl1), bf2x87::Real(xl0)), bf2x87::Real(f)), bf2x87::Real(xl0)));
			int	xl = bf2x87::toIntTruncate(std::ceil(left));
			const double right = bf2x87::storeDouble(bf2x87::add(bf2x87::mul(
				bf2x87::sub(bf2x87::Real(xr1), bf2x87::Real(xr0)), bf2x87::Real(f)), bf2x87::Real(xr0)));
			int	xr = bf2x87::toIntTruncate(std::ceil(right));
			
			xl = iclamp(xl, 0, s_glyph_render_size - 1);
			xr = iclamp(xr, 0, s_glyph_render_size - 1);

			if (xr > xl)
			{
				memset(s_render_buffer + y * s_glyph_render_size + xl,
				       255,
				       xr - xl);
			}
		}
	}


	// Implicit vptr-only constructor: SwiffPlayer.dll 0x100A50A0.
	struct draw_into_software_buffer : tesselate::trapezoid_accepter
	// A trapezoid accepter that does B&W rendering into our
	// software buffer.
	{
		// Overrides from trapezoid_accepter
		// SwiffPlayer.dll 0x100A50B0: transform right edges before left edges and Y.
		virtual void	accept_trapezoid(int style, const tesselate::trapezoid& tr)
		{
			// Transform the coords.
			float	x_scale = s_render_matrix.m_[0][0];
			float	y_scale = s_render_matrix.m_[1][1];
			float	x_offset = s_render_matrix.m_[0][2];
			float	y_offset = s_render_matrix.m_[1][2];

			float	rx1 = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(tr.m_rx1), bf2x87::Real(x_scale)), bf2x87::Real(x_offset)));
			float	rx0 = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(tr.m_rx0), bf2x87::Real(x_scale)), bf2x87::Real(x_offset)));
			float	lx1 = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(tr.m_lx1), bf2x87::Real(x_scale)), bf2x87::Real(x_offset)));
			float	lx0 = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(tr.m_lx0), bf2x87::Real(x_scale)), bf2x87::Real(x_offset)));
			float	y1 = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(tr.m_y1), bf2x87::Real(y_scale)), bf2x87::Real(y_offset)));
			float	y0 = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(tr.m_y0), bf2x87::Real(y_scale)), bf2x87::Real(y_offset)));

			// Draw into the software buffer.
			software_trapezoid(y0, y1, lx0, lx1, rx0, rx1);
		}

		// SwiffPlayer.dll 0x100A52A0: release builds discard glyph outlines' line strips.
		virtual void	accept_line_strip(int style, const point coords[], int coord_count)
		{
			assert(0);	// Shape glyphs should not contain lines.
		}
	};


	// SwiffPlayer.dll 0x100A5C20.
	static bool	render_glyph(rendered_glyph_info* rgi, const shape_character_def* sh)
	// Render the given outline shape into a cached font texture.
	// Return true if the glyph is not empty; false if it's
	// totally empty.
	// 
	// Return fill in the image and offset members of the given
	// rgi.
	{
		assert(rgi);
		assert(sh);
		assert(s_render_buffer);

		//
		// Tesselate and render the shape into a software buffer.
		//

		// Clear the render output to 0.
		memset(s_render_buffer, 0, s_glyph_render_size * s_glyph_render_size);

		// Look at glyph bounds; adjust origin to make sure
		// the shape will fit in our output.
		float	offset_x = 0.f;
		float	offset_y = s_rendering_box;
		rect	glyph_bounds;
		sh->compute_bound(&glyph_bounds);
		if (glyph_bounds.m_x_min < 0)
		{
			offset_x = - glyph_bounds.m_x_min;
		}
		if (glyph_bounds.m_y_max > 0)
		{
			offset_y = s_rendering_box - glyph_bounds.m_y_max;
		}

		s_render_matrix.set_identity();
		s_render_matrix.concatenate_scale(bf2x87::store(bf2x87::div(
			bf2x87::Real(s_glyph_render_size), bf2x87::Real(s_rendering_box))));
		s_render_matrix.concatenate_translation(offset_x, offset_y);

		// Tesselate & draw the shape.
		draw_into_software_buffer	accepter;
		sh->tesselate(bf2x87::store(bf2x87::mul(bf2x87::div(
			bf2x87::Real(s_rendering_box), bf2x87::Real(s_glyph_render_size)), bf2x87::Real(0.5f))), &accepter);

		//
		// Process the results of rendering.
		//

		// Shrink the results down by a factor of 4x, to get
		// antialiasing.  Also, analyze the data boundaries.
		bool	any_nonzero_pixels = false;
		int	min_x = s_glyph_nominal_size;
		int	max_x = 0;
		int	min_y = s_glyph_nominal_size;
		int	max_y = 0;
		Uint8*	output = new Uint8[s_glyph_nominal_size * s_glyph_nominal_size];
		for (int j = 0; j < s_glyph_nominal_size; j++)
		{
			for (int i = 0; i < s_glyph_nominal_size; i++)
			{
				// Sum up the contribution to this output texel.
				int	sum = 0;
				for (int jj = 0; jj < OVERSAMPLE_FACTOR; jj++)
				{
					for (int ii = 0; ii < OVERSAMPLE_FACTOR; ii++)
					{
						Uint8	texel = s_render_buffer[
							((j << OVERSAMPLE_BITS) + jj) * s_glyph_render_size
							+ ((i << OVERSAMPLE_BITS) + ii)];
						sum += texel;
					}
				}
				sum >>= OVERSAMPLE_BITS;
				sum >>= OVERSAMPLE_BITS;
				if (sum > 0)
				{
					any_nonzero_pixels = true;
					min_x = imin(min_x, i);
					max_x = imax(max_x, i);
					min_y = imin(min_y, j);
					max_y = imax(max_y, j);
				}
				output[j * s_glyph_nominal_size + i] = (Uint8) sum;
			}
		}

		if (any_nonzero_pixels)
		{
			// Fill in rendered_glyph_info.
			rgi->m_image = new image::alpha(max_x - min_x + 1, max_y - min_y + 1);
			const bf2x87::Real scale = bf2x87::div(bf2x87::Real(s_glyph_nominal_size), bf2x87::Real(s_rendering_box));
			rgi->m_offset_x = bf2x87::store(bf2x87::sub(bf2x87::mul(bf2x87::Real(offset_x), scale), bf2x87::Real(min_x)));
			rgi->m_offset_y = bf2x87::store(bf2x87::sub(bf2x87::mul(scale, bf2x87::Real(offset_y)), bf2x87::Real(min_y)));

			// Copy the rendered glyph into the new image.
			{for (int j = 0, n = rgi->m_image->m_height; j < n; j++)
			{
				memcpy(
					image::scanline(rgi->m_image, j),
					output + (min_y + j) * s_glyph_nominal_size + min_x,
					rgi->m_image->m_width);
			}}
		}
		else
		{
			// Glyph is empty; don't create an image for it.
			return false;
		}

		delete [] output;	// @@ TODO should keep this around longer, instead of new/delete for each glyph

		rgi->m_image_hash = rgi->m_image->compute_hash();

		return true;
	}



	// SwiffPlayer.dll 0x100A6D60.
	bool	try_to_reuse_previous_image(
		const rendered_glyph_info& rgi,
		const hash<unsigned int, const rendered_glyph_info*>& image_hash)
	// See if we've already packed an identical glyph image for
	// another glyph.  If so, then reuse it, and return true.
	// If no reusable image, return false.
	//
	// Reusing identical images can be a huge win, especially for
	// fonts that use the same dummy glyph for many undefined
	// characters.
	{
		const rendered_glyph_info*	identical_image = NULL;
		if (image_hash.get(rgi.m_image_hash, &identical_image))
		{
			// Found a match.  But is it *really* a match?  Do a
			// bitwise compare.
			if (*(rgi.m_image) == *(identical_image->m_image))
			{
				// Yes, a real bitwise match.  Use the previous
				// image's texture data.
				texture_glyph	identical_tg =
					identical_image->
					m_source_font->
					get_texture_glyph(identical_image->m_glyph_index);

				if (identical_tg.is_renderable() == false)
				{
					// The matching glyph hasn't been pushed into the font yet.
					// Search for it in s_pending_glyphs.
					bool	found_it = false;
					for (int i = 0, n = s_pending_glyphs.size(); i < n; i++)
					{
						const pending_glyph_info&	pgi = s_pending_glyphs[i];
						if (pgi.m_source_font == identical_image->m_source_font
						    && pgi.m_glyph_index == identical_image->m_glyph_index)
						{
							// This is the one we want to alias with.
							identical_tg = pgi.m_texture_glyph;
							found_it = true;
						}
					}

					if (found_it == false)
					{
						// Should not happen -- glyph should either be in the font, or in s_pending_glyphs.
						assert(0);
						return false;
					}
				}

				texture_glyph	tg;

				// copy the bitmap & uv data from identical_tg
				tg = identical_tg;

				// Use our own offset, in case it's different.
				tg.m_uv_origin.m_x = tg.m_uv_bounds.m_x_min
					+ rgi.m_offset_x / GLYPH_CACHE_TEXTURE_SIZE;
				tg.m_uv_origin.m_y = tg.m_uv_bounds.m_y_min
					+ rgi.m_offset_y / GLYPH_CACHE_TEXTURE_SIZE;

				if (identical_tg.is_renderable())
				{
					// This image is already packed and has a valid bitmap_info.
					// Push straight into our font.
					rgi.m_source_font->add_texture_glyph(rgi.m_glyph_index, tg);
				}
				else
				{
					// Set bitmap_info and push into font once texture is done being packed.
					s_pending_glyphs.push_back(
						pending_glyph_info(
							rgi.m_source_font,
							rgi.m_glyph_index,
							tg));
				}

				return true;
			}
			// else hash matched, but images didn't.
		}
		else
		{
#if 0
#ifndef NDEBUG
			// Sanity check the hash -- there should be no
			// image in it that exactly matches this
			// image.
			for (hash<unsigned int, const rendered_glyph_info*>::const_iterator it = image_hash.begin();
			     it != image_hash.end();
			     ++it)
			{
				if (*(rgi.m_image) == *(it->second->m_image))
				{
					// bah!  what up???
					unsigned int	hash_a = rgi.m_image->compute_hash();
					unsigned int	hash_b = it->second->m_image->compute_hash();

					log_msg("a = %x, b = %x\n", hash_a, hash_b);//xxxxx
				}
			}
#endif // not NDEBUG
#endif // 0
		}

		return false;
	}


	// SwiffPlayer.dll 0x100A6F30.
	void	pack_and_assign_glyphs(array<rendered_glyph_info>* glyph_info, movie_definition_sub* owner)
	// Pack the given glyphs into textures, and push the
	// texture_glyph info into the source fonts.
	//
	// Re-arranges the glyphs (i.e. sorts them by size) but
	// otherwise doesn't munge the array.
	{
		// Sort the glyphs by size (biggest first).
		struct sorter
		{
			// SwiffPlayer.dll 0x100A4960; images are bounded by the glyph cache size.
			static int	sort_by_size(const void* a, const void* b)
			// For qsort.
			{
				const rendered_glyph_info*	ga = (const rendered_glyph_info*) a;
				const rendered_glyph_info*	gb = (const rendered_glyph_info*) b;

				int	a_size = ga->m_image->m_width + ga->m_image->m_height;
				int	b_size = gb->m_image->m_width + gb->m_image->m_height;

				return b_size - a_size;
			}
		};
		if (glyph_info->size())
		{
			qsort(&(*glyph_info)[0], glyph_info->size(), sizeof((*glyph_info)[0]), sorter::sort_by_size);
		}

		// Flag for whether we've processed this glyph yet.
		array<bool>	packed;
		packed.resize(glyph_info->size());
		for (int i = 0, n = packed.size(); i < n; i++)
		{
			packed[i] = false;
		}

		// Share identical texture data where possible, by
		// doing glyph image comparisons.
		hash<unsigned int, const rendered_glyph_info*>	image_hash;

		// Pack the glyphs.
		{for (int i = 0, n = glyph_info->size(); i < n; )
		{
			int	index = i;

			// Try to pack a glyph into the existing texture.
			for (;;)
			{
				const rendered_glyph_info&	rgi = (*glyph_info)[index];

				// First things first: are we identical to a glyph that has
				// already been packed?
				if (try_to_reuse_previous_image(rgi, image_hash))
				{
					packed[index] = true;
					break;
				}

				int	raw_width = rgi.m_image->m_width;
				int	raw_height = rgi.m_image->m_height;

				// Need to pad around the outside.
				int	width = raw_width + (PAD_PIXELS * 2);
				int	height = raw_height + (PAD_PIXELS * 2);

				assert(width < GLYPH_CACHE_TEXTURE_SIZE);
				assert(height < GLYPH_CACHE_TEXTURE_SIZE);

				// Does this glyph fit?
				int	pack_x = 0, pack_y = 0;
				ensure_cache_image_available();
				if (pack_rectangle(&pack_x, &pack_y, width, height))
				{
					// Fits!
					// Blit the output image into its new spot.
					for (int j = 0; j < raw_height; j++)
					{
						memcpy(s_current_cache_image
						       + (pack_y + PAD_PIXELS + j) * GLYPH_CACHE_TEXTURE_SIZE
						       + pack_x + PAD_PIXELS,
						       image::scanline(rgi.m_image, j),
						       raw_width);
					}

					// Fill out the glyph info.
					texture_glyph	tg;
					tg.m_uv_origin.m_x = (pack_x + rgi.m_offset_x) / (GLYPH_CACHE_TEXTURE_SIZE);
					tg.m_uv_origin.m_y = (pack_y + rgi.m_offset_y) / (GLYPH_CACHE_TEXTURE_SIZE);
					tg.m_uv_bounds.m_x_min = float(pack_x) / (GLYPH_CACHE_TEXTURE_SIZE);
					tg.m_uv_bounds.m_x_max = float(pack_x + width) / (GLYPH_CACHE_TEXTURE_SIZE);
					tg.m_uv_bounds.m_y_min = float(pack_y) / (GLYPH_CACHE_TEXTURE_SIZE);
					tg.m_uv_bounds.m_y_max = float(pack_y + height) / (GLYPH_CACHE_TEXTURE_SIZE);

					// Fill in bitmap info and push into the source font later.
					s_pending_glyphs.push_back(
						pending_glyph_info(
							rgi.m_source_font,
							rgi.m_glyph_index,
							tg));

					// Add this into the hash so it can possibly be reused.
					if (image_hash.get(rgi.m_image_hash, NULL) == false)
					{
						image_hash.add(rgi.m_image_hash, &rgi);
					}

					packed[index] = true;

					break;
				}
				else
				{
					// Try the next unpacked glyph.
					index++;
					while (index < n && packed[index]) index++;

					if (index >= n)
					{
						// None of the glyphs will fit.  Finish off this texture.
						finish_current_texture(owner);

						// And go around again.
						index = i;
					}
				}
			}

			// Skip to the next unpacked glyph.
			while (i < n && packed[i]) i++;
		}}
	}


	// SwiffPlayer.dll 0x100A6A40.
	static void	generate_font_bitmaps(array<rendered_glyph_info>* glyph_info, font* f, movie_definition_sub* owner)
	// Render images for each of the font's glyphs, and put the
	// info about them in the given array.
	{
		assert(glyph_info);
		assert(f);

		f->set_texture_glyph_nominal_size(s_glyph_nominal_size);

		for (int i = 0, n = f->get_glyph_count(); i < n; i++)
		{
			if (f->get_texture_glyph(i).is_renderable() == false)
			{
				shape_character_def*	sh = f->get_glyph(i);
				if (sh)
				{
					rect	glyph_bounds;
					sh->compute_bound(&glyph_bounds);

					if (glyph_bounds.width() < 0)
					{
						// Invalid width; this must be an empty glyph.
						// Don't bother generating a texture for it.
					}
					else
					{
						// Add a glyph.
						rendered_glyph_info	rgi;
						rgi.m_source_font = f;
						rgi.m_glyph_index = i;

						if (render_glyph(&rgi, sh) == true)
						{
							glyph_info->push_back(rgi);
						}
						// else glyph is empty
					}
				}
			}
		}
	}


	// SwiffPlayer.dll 0x100A4980: divide the nominal size before multiplying by 1024.
	float	get_texture_glyph_max_height(const font* f)
	{
		return bf2x87::store(bf2x87::mul(
			bf2x87::div(bf2x87::Real(f->get_texture_glyph_nominal_size()),
				bf2x87::Real(s_rendering_box)), bf2x87::Real(1024.0f)));
	}


	// SwiffPlayer.dll 0x100A52E0.
	static void	wipe_font_textures(const array<font*>& fonts)
	{
		for (int i = 0, n = fonts.size(); i < n; i++)
		{
			font*	f = fonts[i];
			f->wipe_texture_glyphs();
		}
	}


	//
	// Public interface
	//


	// SwiffPlayer.dll 0x100A7300.
	void	generate_font_bitmaps(const array<font*>& fonts, movie_definition_sub* owner)
	// Build cached textures from glyph outlines.
	{
		assert(s_render_buffer == NULL);
		s_render_buffer = new Uint8[s_glyph_render_size * s_glyph_render_size];

		// Build the glyph images.
		array<rendered_glyph_info>	glyph_info;
		for (int i = 0; i < fonts.size(); i++)
		{
			generate_font_bitmaps(&glyph_info, fonts[i], owner);
		}

		// Pack all the rendered glyphs and push the info into their fonts.
		pack_and_assign_glyphs(&glyph_info, owner);

		// Delete glyph images.
		{for (int i = 0, n = glyph_info.size(); i < n; i++)
		{
			delete glyph_info[i].m_image;
		}}
		glyph_info.clear();

		// Finish off any pending cache texture.
		finish_current_texture(owner);

		// Clean up our buffers.
		if (s_current_cache_image)
		{
			delete [] s_current_cache_image;
			s_current_cache_image = NULL;

			s_covered_rects.resize(0);
			s_anchor_points.resize(0);
		}

		// Clean up the render buffer that we just used.
		assert(s_render_buffer);
		delete [] s_render_buffer;
		s_render_buffer = NULL;
	}


	// SwiffPlayer.dll 0x100A7480.
	void	output_cached_data(
		tu_file* out,
		const array<font*>& fonts,
		movie_definition_sub* owner,
		const cache_options& options)
	// Save cached font data, including glyph textures, to a
	// stream.  This is used by the movie caching code.
	{
		assert(out);

		// skip number of bitmaps.
		int	bitmaps_used_base = owner->get_bitmap_info_count();
		int	size_pos = out->get_position();
		out->write_le16(0);
		
		// save bitmaps
		s_save_dummy_bitmaps = false;
		if (options.m_include_font_bitmaps == false)
		{
			s_save_dummy_bitmaps = true;	// HACK!!!
		}
		s_file = out;			// HACK!!!
		s_saving = true;		// HACK!!!
		wipe_font_textures(fonts);	// HACK -- eliminate the font textures, so generate_font_bitmaps() regen's them.
		generate_font_bitmaps(fonts, owner);
		s_saving = false;
		s_file = NULL;
		
		// save number of bitmaps.
		out->set_position(size_pos);
		out->write_le16(owner->get_bitmap_info_count() - bitmaps_used_base);
		out->go_to_end();
		
		// save number of fonts.
		out->write_le16(fonts.size());
		
		// for each font:
		for (int f = 0; f < fonts.size(); f++)
		{
			font*	fnt = fonts[f];

			// Write out the nominal glyph size for this font (for calibrating scaling).
			int	nominal_glyph_size = fnt->get_texture_glyph_nominal_size();
			out->write_le16(nominal_glyph_size);

			// skip number of glyphs.
			int ng = fonts[f]->get_glyph_count();
			int ng_position = out->get_position();
			out->write_le32(0);
			
			int n = 0;
			
			// save texture glyphs:
			for (int g = 0; g < ng; g++)
			{
				const texture_glyph& tg = fonts[f]->get_texture_glyph(g);
				if (tg.is_renderable())
				{
					// save glyph index.
					out->write_le32(g);

					// save bitmap index.
					// @@ blech, linear search
					int bi;
					for (bi = bitmaps_used_base; bi < owner->get_bitmap_info_count(); bi++)
					{
						if (tg.m_bitmap_info == owner->get_bitmap_info(bi))
						{
							break;
						}
					}
					assert(bi >= bitmaps_used_base
					       && bi < owner->get_bitmap_info_count());

					out->write_le16((Uint16) (bi - bitmaps_used_base));

					// save rect, position.
					out->write_float32(tg.m_uv_bounds.m_x_min);
					out->write_float32(tg.m_uv_bounds.m_y_min);
					out->write_float32(tg.m_uv_bounds.m_x_max);
					out->write_float32(tg.m_uv_bounds.m_y_max);
					out->write_float32(tg.m_uv_origin.m_x);
					out->write_float32(tg.m_uv_origin.m_y);
					n++;
				}
			}

			out->set_position(ng_position);
			out->write_le32(n);
			out->go_to_end();

			// Output cached shape data.
			fonts[f]->output_cached_data(out, options);
		}

		if (out->get_error() != TU_FILE_NO_ERROR)
		{
			log_error("gameswf::fontlib::save_cached_font_data(): problem writing to output stream!");
		}

		// @@ NOTE: should drop any bitmap_info's in owner
		// that have a ref count == 1, since they're orphaned
		// now... or else use weak_ptr's in owner?
	}
	

	// SwiffPlayer.dll 0x100A5ED0. [CHOSEN, not recovered] S3 bridge:
	// use the native tu_file and typed bitmap/font containers for the retail records.
	void	input_cached_data(tu_file* in, const array<font*>& fonts, movie_definition_sub* owner)
	// Load a stream containing previously-saved font glyph textures.
	{
		// load number of bitmaps.
		int nb = in->read_le16();

		int pw = 0, ph = 0;

		// load bitmaps.
		int	bitmaps_used_base = owner->get_bitmap_info_count();
		for (int b = 0; b < nb; b++)
		{
			// load bitmap size
			int w = in->read_le16();
			int h = in->read_le16();

			smart_ptr<bitmap_info>	bi;
			if (owner->get_create_bitmaps() == DO_NOT_LOAD_BITMAPS)
			{
				// Skip image data bytes.
				in->set_position(in->get_position() + w * h);

				// Make a placeholder bitmap.
				bi = render::create_bitmap_info_empty();
			}
			else
			{
				// load bitmap contents
				if (s_current_cache_image == NULL || w != pw || h != ph)
				{
					delete [] s_current_cache_image;
					s_current_cache_image = new Uint8[w*h];
					pw = w;
					ph = h;
				}

				in->read_bytes(s_current_cache_image, w * h);

				bi = render::create_bitmap_info_alpha(
					w,
					h,
					s_current_cache_image);
			}
			owner->add_bitmap_info(bi.get_ptr());
			assert(bi->get_ref_count() == 2);	// one ref for bi, one for the owner.
		}

		delete [] s_current_cache_image;
		s_current_cache_image = NULL;

		// load number of fonts.
		int nf = in->read_le16();
		if (nf != fonts.size())
		{
			// Font counts must match!
			log_error("error: mismatched font count (read %d, expected %d) in cached font data\n", nf, fonts.size());
			in->go_to_end();
			goto error_exit;
		}

		// for each font:
		{for (int f = 0; f < nf; f++)
		{
			font*	fnt = fonts[f];

			if (in->get_error() != TU_FILE_NO_ERROR)
			{
				log_error("error reading cache file (fonts); skipping\n");
				return;
			}
			if (in->get_eof())
			{
				log_error("unexpected eof reading cache file (fonts); skipping\n");
				return;
			}

			// Get nominal glyph size for this font (for calibrating scaling).
			int	nominal_glyph_size = in->read_le16();
			fnt->set_texture_glyph_nominal_size(nominal_glyph_size);

			// load number of texture glyphs.
			int ng = in->read_le32();

			// load glyphs:
			for (int g=0; g<ng; g++)
			{
				// load glyph index.
				int glyph_index = in->read_le32();

				texture_glyph	tg;

				// load bitmap index
				int bi = in->read_le16();
				if (bi + bitmaps_used_base >= owner->get_bitmap_info_count())
				{
					// Bad data; give up.
					log_error("error: invalid bitmap index %d in cached font data\n", bi);
					in->go_to_end();
					goto error_exit;
				}

				tg.set_bitmap_info(owner->get_bitmap_info(bi + bitmaps_used_base));

				// load glyph bounds and origin.
				tg.m_uv_bounds.m_x_min = in->read_float32();
				tg.m_uv_bounds.m_y_min = in->read_float32();
				tg.m_uv_bounds.m_x_max = in->read_float32();
				tg.m_uv_bounds.m_y_max = in->read_float32();
				tg.m_uv_origin.m_x = in->read_float32();
				tg.m_uv_origin.m_y = in->read_float32();

				fnt->add_texture_glyph(glyph_index, tg);
			}

			// Load cached shape data.
			fnt->input_cached_data(in);
		}}

	error_exit:
		;

	}


	// SwiffPlayer.dll 0x100A6AE0.
	void	clear()
	// Release all the fonts we know about.
	{
		s_fonts.clear();
		openbf2::clearMovieGlyphs();
	}


	// SwiffPlayer.dll 0x100A5310.
	int	get_font_count()
	// Return the number of fonts in our library.
	{
		return s_fonts.size();
	}


	// SwiffPlayer.dll 0x100A5320.
	font*	get_font(int index)
	// Retrieve one of our fonts, by index.
	{
		if (index < 0 || index >= s_fonts.size())
		{
			return NULL;
		}

		return s_fonts[index].get_ptr();
	}


	// SwiffPlayer.dll 0x100A5340: live array bound, null entries skipped, strcmp.
	font*	get_font(const char* name)
	// Return the named font.
	{
		// Dumb linear search.
		for (int i = 0; i < s_fonts.size(); i++)
		{
			font*	f = s_fonts[i].get_ptr();
			if (f != NULL)
			{
				if (strcmp(f->get_name(), name) == 0)
				{
					return f;
				}
			}
		}
		return NULL;
	}
			

	// SwiffPlayer.dll 0x100A49C0.
	const char*	get_font_name(const font* f)
	// Return the name of the given font.  (This basically exists
	// so that font* can be opaque to the host app).
	{
		if (f == NULL)
		{
			return "<null>";
		}
		return f->get_name();
	}


	// SwiffPlayer.dll 0x100A6AF0; push_back constructs a temporary strong pointer.
	void	add_font(font* f)
	// Add the given font to our library.
	{
		assert(f);

#ifndef NDEBUG
		// Make sure font isn't already in the list.
		for (int i = 0; i < s_fonts.size(); i++)
		{
			assert(s_fonts[i] != f);
		}
#endif // not NDEBUG

		s_fonts.push_back(f);
	}


	// SwiffPlayer.dll 0x100A53C0.
	void	draw_glyph(const matrix& mat, const texture_glyph& tg, rgba color, int nominal_glyph_height)
	// Draw the given texture glyph using the given transform, in
	// the given color.
	{
		assert(tg.is_renderable());

		// @@ worth it to precompute these bounds?

		rect	bounds = tg.m_uv_bounds;
		const bf2x87::Real x_min = bf2x87::sub(bf2x87::Real(bounds.m_x_min), bf2x87::Real(tg.m_uv_origin.m_x));
		const bf2x87::Real x_max = bf2x87::sub(bf2x87::Real(bounds.m_x_max), bf2x87::Real(tg.m_uv_origin.m_x));
		bounds.m_y_min = bf2x87::store(bf2x87::sub(bf2x87::Real(bounds.m_y_min), bf2x87::Real(tg.m_uv_origin.m_y)));
		bounds.m_y_max = bf2x87::store(bf2x87::sub(bf2x87::Real(bounds.m_y_max), bf2x87::Real(tg.m_uv_origin.m_y)));

		// Scale from uv coords to the 1024x1024 glyph square.
		// @@ need to factor this out!
		static float	s_scale = bf2x87::store(bf2x87::div(bf2x87::mul(
			bf2x87::Real(s_rendering_box), bf2x87::Real(float(GLYPH_CACHE_TEXTURE_SIZE))), bf2x87::Real(nominal_glyph_height)));

		bounds.m_x_min = bf2x87::store(bf2x87::mul(bf2x87::Real(s_scale), x_min));
		bounds.m_x_max = bf2x87::store(bf2x87::mul(bf2x87::Real(s_scale), x_max));
		bounds.m_y_min = bf2x87::store(bf2x87::mul(bf2x87::Real(s_scale), bf2x87::Real(bounds.m_y_min)));
		bounds.m_y_max = bf2x87::store(bf2x87::mul(bf2x87::Real(s_scale), bf2x87::Real(bounds.m_y_max)));
		
		render::draw_bitmap(mat, tg.m_bitmap_info.get_ptr(), bounds, tg.m_uv_bounds, color);
	}


#if 0
	void	draw_string(const font* f, float x, float y, float size, const char* text)
	// Host-driven text rendering function. This not-tested and unfinished.
	{
		// Dummy arrays with a white fill style.  For passing to shape_character::display().
		static array<fill_style>	s_dummy_style;
		static array<line_style>	s_dummy_line_style;
		static display_info	s_dummy_display_info;
		if (s_dummy_style.size() < 1)
		{
			s_dummy_style.resize(1);
			s_dummy_style.back().set_color(rgba(255, 255, 255, 255));
		}

		// Render each glyph in the string.
		for (int i = 0; text[i]; i++)
		{
			int g = f->get_glyph_index(text[i]);
			if (g == -1) 
			{
				continue;	// FIXME: advance?
			}

			const texture_glyph&	tg = f->get_texture_glyph(g);
			
			matrix m;
			m.concatenate_translation(x, y);
			m.concatenate_scale(size / 1024.0f);

			if (tg.is_renderable())
			{
				// Draw the glyph using the cached texture-map info.
				fontlib::draw_glyph(m, tg, rgba());
			}
			else
			{
				shape_character_def*	glyph = f->get_glyph(g);

				// Draw the character using the filled outline.
				if (glyph)
				{
					glyph->display(dummy_inst, s_dummy_style, s_dummy_line_style);
				}
			}

			x += f->get_advance(g);
		}

	}
#endif // 0


	// [CHOSEN, not recovered] Movies own their fonts. The renderer can borrow
	// outlines while loading an atlas without extending the movie's GPU lifetime.
	using HiDpiFontList = std::vector<weak_ptr<font>>;
	using HiDpiFontRegistry = std::map<std::string, HiDpiFontList>;
	static HiDpiFontRegistry& hidpi_registry()
	{
		static HiDpiFontRegistry registry;
		return registry;
	}

	static void pruneExpiredFonts(HiDpiFontList& fonts)
	{
		fonts.erase(std::remove_if(fonts.begin(), fonts.end(), [](weak_ptr<font>& entry) {
			return entry == static_cast<font*>(nullptr);
		}), fonts.end());
	}

	static unsigned int&	hidpi_generation()
	{
		static unsigned int	s_generation = 0;
		return s_generation;
	}

	static std::string	hidpi_key(const char* name)
	{
		std::string	key(name ? name : "");
		for (size_t i = 0; i < key.size(); i++)
		{
			key[i] = (char) tolower((unsigned char) key[i]);
		}
		return key;
	}

	void	hidpi_register_exported_font(const char* export_name, font* f)
	{
		if (f == NULL || f->get_glyph_count() == 0)
		{
			return;
		}
		HiDpiFontList& list = hidpi_registry()[hidpi_key(export_name)];
		pruneExpiredFonts(list);
		for (size_t i = 0; i < list.size(); i++)
		{
			if (list[i] == f) return;
		}
		list.push_back(f);
		++hidpi_generation();
	}

	// Keep one font per name for atlas matching when there is no matching SWF export.
	void	hidpi_register_font(font* f)
	{
		if (f == NULL || f->get_glyph_count() == 0)
		{
			return;
		}
		HiDpiFontList& list = hidpi_registry()["*"];
		pruneExpiredFonts(list);
		const std::string	name(f->get_name() ? f->get_name() : "");
		for (size_t i = 0; i < list.size(); i++)
		{
			smart_ptr<font> g = list[i];
			if (g.get_ptr() == f) return;
			if (name == (g->get_name() ? g->get_name() : ""))
			{
				if (f->get_glyph_count() > g->get_glyph_count())
				{
					list[i] = f;
					++hidpi_generation();
				}
				return;
			}
		}
		list.push_back(f);
		++hidpi_generation();
	}

	static smart_ptr<font> hidpi_font(const char* export_name, int index)
	{
		auto it =
			hidpi_registry().find(hidpi_key(export_name));
		if (it == hidpi_registry().end() || index < 0 || index >= (int) it->second.size())
		{
			return NULL;
		}
		return it->second[index];
	}

	// Union of the tesselator's fill trapezoids on a supersampled grid (sample centres at
	// +0.5); the glyph outlines are single-style fills, as render_glyph assumes.
	struct hidpi_coverage_accepter : tesselate::trapezoid_accepter
	{
		unsigned char*	m_samples;
		int	m_w, m_h;		// grid size in samples
		float	m_scale;		// samples per EM unit
		float	m_ox, m_oy;		// origin in samples

		virtual void	accept_trapezoid(int style, const tesselate::trapezoid& tr)
		{
			const float	y0 = tr.m_y0 * m_scale + m_oy;
			const float	y1 = tr.m_y1 * m_scale + m_oy;
			if (!(y1 > y0)) return;
			int	j0 = (int) ceilf(y0 - 0.5f);
			int	j1 = (int) ceilf(y1 - 0.5f);
			if (j0 < 0) j0 = 0;
			if (j1 > m_h) j1 = m_h;
			for (int j = j0; j < j1; j++)
			{
				const float	f = ((float) j + 0.5f - y0) / (y1 - y0);
				const float	xl = (tr.m_lx0 + (tr.m_lx1 - tr.m_lx0) * f) * m_scale + m_ox;
				const float	xr = (tr.m_rx0 + (tr.m_rx1 - tr.m_rx0) * f) * m_scale + m_ox;
				int	i0 = (int) ceilf(xl - 0.5f);
				int	i1 = (int) ceilf(xr - 0.5f);
				if (i0 < 0) i0 = 0;
				if (i1 > m_w) i1 = m_w;
				if (i1 > i0) memset(m_samples + j * m_w + i0, 1, i1 - i0);
			}
		}

		virtual void	accept_line_strip(int style, const point coords[], int coord_count)
		{
		}
	};

	bool	hidpi_raster_glyph(const char* export_name, int index, Uint16 code, float em_px,
				   float ox, float oy, float* out, int w, int h, int ss)
	{
		if (out == NULL || w <= 0 || h <= 0) return false;
		memset(out, 0, sizeof(float) * w * h);
		smart_ptr<font> f = hidpi_font(export_name, index);
		if (!f.get_ptr() || ss < 1 || !(em_px > 0.0f)) return false;
		const int	glyph_index = f->get_glyph_index(code);
		if (glyph_index < 0) return false;
		shape_character_def*	glyph = f->get_glyph(glyph_index);
		if (glyph == NULL) return false;

		const int	sw = w * ss;
		const int	sh = h * ss;
		std::vector<unsigned char>	samples(sw * sh, 0);
		hidpi_coverage_accepter	accepter;
		accepter.m_samples = &samples[0];
		accepter.m_w = sw;
		accepter.m_h = sh;
		accepter.m_scale = em_px * (float) ss / 1024.0f;	// DefineFont2: 1024 units per EM
		accepter.m_ox = ox * (float) ss;
		accepter.m_oy = oy * (float) ss;
		// A tenth of a sample of curve error.
		glyph->tesselate(0.1f / accepter.m_scale, &accepter);

		const float	inv = 1.0f / (float) (ss * ss);
		bool	any = false;
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				int	sum = 0;
				for (int j = 0; j < ss; j++)
				{
					const unsigned char*	row = &samples[(y * ss + j) * sw + x * ss];
					for (int i = 0; i < ss; i++) sum += row[i];
				}
				out[y * w + x] = (float) sum * inv;
				if (sum) any = true;
			}
		}
		return any;
	}

}	// end namespace fontlib
}	// end namespace gameswf


// [CHOSEN, not recovered] The renderer-facing half of SwiffPlayerAbi/HiDpiGlyphs.h.
static unsigned int glyphGeneration()
{
	return gameswf::fontlib::hidpi_generation();
}

static const char* glyphName(const char* exportName, int index)
{
	smart_ptr<gameswf::font> f = gameswf::fontlib::hidpi_font(exportName, index);
	return f.get_ptr() && f->get_name() ? f->get_name() : "";
}

static int glyphCandidates(const char* exportName)
{
	auto it =
		gameswf::fontlib::hidpi_registry().find(gameswf::fontlib::hidpi_key(exportName));
	if (it == gameswf::fontlib::hidpi_registry().end()) return 0;
	gameswf::fontlib::pruneExpiredFonts(it->second);
	return static_cast<int>(it->second.size());
}

static bool rasterGlyph(const char* exportName, int index, unsigned short code,
					float emPx, float ox, float oy, float* out, int w, int h, int ss)
{
	return gameswf::fontlib::hidpi_raster_glyph(exportName, index, code, emPx, ox, oy, out, w, h, ss);
}


namespace openbf2 {
HiDpiGlyphs& movieGlyphs()
{
    static HiDpiGlyphs glyphs = { &glyphGeneration, &glyphCandidates, &glyphName, &rasterGlyph };
    return glyphs;
}

void clearMovieGlyphs()
{
    gameswf::fontlib::hidpi_registry().clear();
    ++gameswf::fontlib::hidpi_generation();
}
} // namespace openbf2

// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
