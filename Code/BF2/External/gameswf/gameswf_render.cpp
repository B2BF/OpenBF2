// gameswf_render.cpp	-- Willem Kokke <willem@mindparity.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Interface to back-end renderer.


#include <cstdio>
#include <cstdlib>
#include <new>
#include "gameswf_render.h"
#include "gameswf_log.h"
#include "gameswf_profile.h"


namespace gameswf 
{
	static render_handler* s_render_handler;

	// SwiffPlayer.dll 0x100963A0: notify the currently installed handler.
	bitmap_info::~bitmap_info()
	{
		// [CHOSEN, not recovered] Cached generated images release through their
		// own registry leases, never the currently installed player's handler.
		if (!m_render_resource)
			render::delete_bitmap_info(this);
	}


	// SwiffPlayer.dll 0x1009A3F0.
	void set_render_handler(render_handler* r)
	{
		s_render_handler = r;
	}

	// SwiffPlayer.dll 0x1009A400.
	render_handler* get_render_handler()
	{
		return s_render_handler;
	}


	namespace render
	{
		// SwiffPlayer.dll 0x1009A410: the no-handler bitmap has zero texture and dimensions.
		// [CHOSEN, not recovered] S3 bridge: native bitmap_info/ref_counted layout.
		struct bogus_bi : public bitmap_info
		{
			// The base destructor sends the notification once (0x1009AA50).
		};


		// SwiffPlayer.dll 0x1009A9D0.
		bitmap_info*	create_bitmap_info_empty()
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_empty();
			else return new (std::nothrow) bogus_bi;
		}

		// SwiffPlayer.dll 0x1009A470.
		bitmap_info*	create_bitmap_info_alpha(int w, int h, unsigned char* data)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_alpha(w, h, data);
			else return new (std::nothrow) bogus_bi;
		}

		// SwiffPlayer.dll 0x1009A500.
		bitmap_info*	create_bitmap_info_rgb(image::rgb* im)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_rgb(im);
			else return new (std::nothrow) bogus_bi;
		}

		// SwiffPlayer.dll 0x1009A590.
		bitmap_info*	create_bitmap_info_rgba(image::rgba* im)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_rgba(im);
			else return new (std::nothrow) bogus_bi;
		}

		// SwiffPlayer.dll 0x1009A620: handler slot 5; preserve its null result.
		bitmap_info*	create_bitmap_info_file(const char* path)
		{
			if (s_render_handler) return s_render_handler->create_bitmap_info_file(path);
			else return new (std::nothrow) bogus_bi;
		}

		// SwiffPlayer.dll 0x1009A6B0.
		void	delete_bitmap_info(bitmap_info* bi)
		{
			if (s_render_handler) s_render_handler->delete_bitmap_info(bi);
		}

		// Bracket the displaying of a frame from a movie.
		// Fill the background color, and set up default
		// transforms, etc.
		// SwiffPlayer.dll 0x1009AA80.
		void	begin_display(
			rgba background_color,
			int viewport_x0, int viewport_y0,
			int viewport_width, int viewport_height,
			float x0, float x1, float y0, float y1)
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("beginDisplay");
					counter.begin();
					s_render_handler->begin_display(
						background_color, viewport_x0, viewport_y0,
						viewport_width, viewport_height, x0, x1, y0, y1);
					counter.end();
					return;
				}
				s_render_handler->begin_display(
					background_color, viewport_x0, viewport_y0,
					viewport_width, viewport_height,
					x0, x1, y0, y1);
			}
// 			else
// 			{
// 				log_error("begin_display called, but no render_handler was registered by the app!\n");
// 			}
		}


		// SwiffPlayer.dll 0x1009AB60.
		void	end_display()
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("endDisplay");
					counter.begin();
					s_render_handler->end_display();
					counter.end();
				}
				else s_render_handler->end_display();
			}
		}


		// Geometric and color transforms for mesh and line_strip rendering.
		// SwiffPlayer.dll 0x1009A6D0.
		void	set_matrix(const matrix& m)
		{
			if (s_render_handler) s_render_handler->set_matrix(m);
		}
		// SwiffPlayer.dll 0x1009A6F0.
		void	set_cxform(const cxform& cx)
		{
			if (s_render_handler) s_render_handler->set_cxform(cx);
		}

		// SwiffPlayer.dll 0x1009A710 guards the handler and forwards its integer.
		void	set_depth(int depth)
		{
			if (s_render_handler)
			{
				s_render_handler->set_depth(depth);
			}
		}

		// Draw triangles using the current fill-style 0.
		// Clears the style list after rendering.
		//
		// coords is a list of (x,y) coordinate pairs, in
		// triangle-strip order.  The type of the array should
		// be float[vertex_count*2]
		// SwiffPlayer.dll 0x1009ABF0.
		void	draw_mesh_strip(const Sint16 coords[], int vertex_count)
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("drawMeshStrip");
					counter.begin();
					s_render_handler->draw_mesh_strip(coords, vertex_count);
					counter.end();
				}
				else s_render_handler->draw_mesh_strip(coords, vertex_count);
			}
		}

		// Draw a line-strip using the current line style.
		// Clear the style list after rendering.
		//
		// Coords is a list of (x,y) coordinate pairs, in
		// sequence.
		// SwiffPlayer.dll 0x1009AC90.
		void	draw_line_strip(const Sint16 coords[], int vertex_count)
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("drawLineStrip");
					counter.begin();
					s_render_handler->draw_line_strip(coords, vertex_count);
					counter.end();
				}
				else s_render_handler->draw_line_strip(coords, vertex_count);
			}
		}

// 		// Set line and fill styles for mesh & line_strip
// 		// rendering.
// 		enum bitmap_wrap_mode
// 		{
// 			WRAP_REPEAT,
// 			WRAP_CLAMP
// 		};

		// SwiffPlayer.dll 0x1009A730.
		void	fill_style_disable(int fill_side)
		{
			if (s_render_handler) s_render_handler->fill_style_disable(fill_side);
		}

		// SwiffPlayer.dll 0x1009A750.
		void	fill_style_color(int fill_side, rgba color)
		{
			if (s_render_handler) s_render_handler->fill_style_color(fill_side, color);
		}

		// SwiffPlayer.dll 0x1009A770.
		void	fill_style_bitmap(int fill_side, const bitmap_info* bi, const matrix& m, render_handler::bitmap_wrap_mode wm)
		{
			if (s_render_handler) s_render_handler->fill_style_bitmap(fill_side, bi, m, wm);
		}

		// SwiffPlayer.dll 0x1009A7A0.
		void	line_style_disable()
		{
			if (s_render_handler) s_render_handler->line_style_disable();
		}

		// SwiffPlayer.dll 0x1009A7B0.
		void	line_style_color(rgba color)
		{
			if (s_render_handler) s_render_handler->line_style_color(color);
		}

		// SwiffPlayer.dll 0x1009A7D0.
		void	line_style_width(float width)
		{
			if (s_render_handler) s_render_handler->line_style_width(width);
		}

		// SwiffPlayer.dll 0x1009A7F0.
		void	begin_submit_mask()
		{
			if (s_render_handler) s_render_handler->begin_submit_mask();
		}

		// SwiffPlayer.dll 0x1009A810.
		void	end_submit_mask()
		{
			if (s_render_handler) s_render_handler->end_submit_mask();
		}

		// SwiffPlayer.dll 0x1009A830.
		void	disable_mask()
		{
			if (s_render_handler) s_render_handler->disable_mask();
		}

		//  the text-line slots; each shipped forwarder guards on the handler
		// exactly like these.
		// SwiffPlayer.dll 0x1009A850.
		void*	text_create(const char* font_name, int height_px)
		{
			return s_render_handler ? s_render_handler->text_create(font_name, height_px) : NULL;
		}
		// SwiffPlayer.dll 0x1009A890.
		void	text_release(void* line)
		{
			if (s_render_handler) s_render_handler->text_release(line);
		}
		// SwiffPlayer.dll 0x1009ADF0; native wchar_t text is the existing host bridge.
		void	text_set(void* line, const wchar_t* text)
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("setText");
					counter.begin();
					s_render_handler->text_set(line, text);
					counter.end();
				}
				else s_render_handler->text_set(line, text);
			}
		}
		// SwiffPlayer.dll 0x1009A870.
		void	text_detach(void* line)
		{
			if (s_render_handler) s_render_handler->text_detach(line);
		}
		// SwiffPlayer.dll 0x1009AE90.
		void	text_draw(void* line, const matrix* m, unsigned color, int flags)
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("drawText");
					counter.begin();
					s_render_handler->text_draw(line, m, color, flags);
					counter.end();
				}
				else s_render_handler->text_draw(line, m, color, flags);
			}
		}
		// SwiffPlayer.dll 0x1009A8B0.
		bool	text_extent(void* line, int count, float* w, float* h)
		{
			return s_render_handler ? s_render_handler->text_extent(line, count, w, h) : false;
		}
		// 0x1009A8E0: ecx = line, edx = out_pos, the two float dwords passed on to handler +0x6C.
		bool	text_hit(void* line, int* out_pos, float x, float y)
		{
			return s_render_handler ? s_render_handler->text_hit(line, out_pos, x, y) : false;
		}
		// SwiffPlayer.dll 0x1009A910: without a handler the outputs are untouched.
		bool	text_selection(void* line, int* a, int* b)
		{
			return s_render_handler ? s_render_handler->text_selection(line, a, b) : false;
		}
		// 0x1009A940: the four float dwords passed on to handler +0x74.
		bool	text_set_selection(void* line, float x1, float y1, float x2, float y2)
		{
			return s_render_handler ? s_render_handler->text_set_selection(line, x1, y1, x2, y2) : false;
		}
		// SwiffPlayer.dll 0x1009A970.
		void	text_blur(void* line)
		{
			if (s_render_handler) s_render_handler->text_blur(line);
		}
		// SwiffPlayer.dll 0x1009A990.
		void	text_set_size(void* line, float size, int flag)
		{
			if (s_render_handler) s_render_handler->text_set_size(line, size, flag);
		}
		// SwiffPlayer.dll 0x1009A9B0.
		void	text_caret(void* line, int pos)
		{
			if (s_render_handler) s_render_handler->text_caret(line, pos);
		}

		// Special function to draw a rectangular bitmap;
		// intended for textured glyph rendering.  Ignores
		// current transforms.
		// SwiffPlayer.dll 0x1009AD30.
		void	draw_bitmap(const matrix& m, const bitmap_info* bi, const rect& coords, const rect& uv_coords, rgba color)
		{
			if (s_render_handler)
			{
				if (s_profiling)
				{
					static profile_record counter("drawBitmap");
					counter.begin();
					s_render_handler->draw_bitmap(m, bi, coords, uv_coords, color);
					counter.end();
					return;
				}
				s_render_handler->draw_bitmap(
					m, bi, coords, uv_coords, color);
			}
		}
	}
}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
