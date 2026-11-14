// gameswf_text.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Code for the text tags.


#include <cerrno>
#include <clocale>
#include <cstdlib>
#include <cwchar>
#include "base/utf8.h"
#include "../../Platform/X87Compat.h"
#include "base/utility.h"
#include "gameswf_impl.h"
#include "gameswf_shape.h"
#include "gameswf_stream.h"
#include "gameswf_log.h"
#include "gameswf_profile.h"
#include "gameswf_font.h"
#include "gameswf_fontlib.h"
#include "gameswf_render.h"

namespace gameswf
{
	//
	// text_character
	//


	// Helper struct.
	struct text_style
	{
		int	m_font_id;
		mutable font*	m_font;
		rgba	m_color;
		float	m_x_offset;
		float	m_y_offset;
		float	m_text_height;
		bool	m_has_x_offset;
		bool	m_has_y_offset;

		// SwiffPlayer.dll 0x10084DB0.
		text_style()
			:
			m_font_id(-1),
			m_font(NULL),
			m_x_offset(0),
			m_y_offset(0),
			m_text_height(1.0f),
			m_has_x_offset(false),
			m_has_y_offset(false)
		{
		}

		void	resolve_font(movie_definition_sub* root_def) const
		{
			if (m_font == NULL)
			{
				assert(m_font_id >= 0);

				m_font = root_def->get_font(m_font_id);
				if (m_font == NULL)
				{
					log_error("error: text style with undefined font; font_id = %d\n", m_font_id);
				}
			}
		}
	};


	// Helper struct.
	struct text_glyph_record
	{
		struct glyph_entry
		{
			int	m_glyph_index;
			float	m_glyph_advance;
		};
		text_style	m_style;
		array<glyph_entry>	m_glyphs;

		// SwiffPlayer.dll 0x100864E0 / 0x10086520; deleting entry 0x10086640.
		// Native members own their storage.
		text_glyph_record() = default;
		~text_glyph_record() = default;
		// SwiffPlayer.dll 0x10085E80: style assignment followed by a deep glyph-array copy.
		text_glyph_record& operator=(const text_glyph_record&) = default;

		// SwiffPlayer.dll 0x10085990.
		void	read(stream* in, int glyph_count, int glyph_bits, int advance_bits)
		{
			m_glyphs.resize(glyph_count);
			for (int i = 0; i < glyph_count; i++)
			{
				m_glyphs[i].m_glyph_index = in->read_uint(glyph_bits);
				m_glyphs[i].m_glyph_advance = (float) in->read_sint(advance_bits);
			}
		}
	};


	// Render the given glyph records.
	static void	display_glyph_records(
		const matrix& this_mat,
		character* inst,
		const array<text_glyph_record>& records,
		movie_definition_sub* root_def)
	{
		static array<fill_style>	s_dummy_style;	// used to pass a color on to shape_character::display()
		static array<line_style>	s_dummy_line_style;
		s_dummy_style.resize(1);

		matrix	mat = inst->get_world_matrix();
		mat.concatenate(this_mat);

		cxform	cx = inst->get_world_cxform();
		float	pixel_scale = inst->get_pixel_scale();

//		display_info	sub_di = di;
//		sub_di.m_matrix.concatenate(mat);

//		matrix	base_matrix = sub_di.m_matrix;
		matrix	base_matrix = mat;
		float	base_matrix_max_scale = base_matrix.get_max_scale();

		float	scale = 1.0f;
		float	x = 0.0f;
		float	y = 0.0f;

		for (int i = 0; i < records.size(); i++)
		{
			// Draw the characters within the current record; i.e. consecutive
			// chars that share a particular style.
			const text_glyph_record&	rec = records[i];

			rec.m_style.resolve_font(root_def);

			font*	fnt = rec.m_style.m_font;
			if (fnt == NULL)
			{
				continue;
			}

			scale = rec.m_style.m_text_height / 1024.0f;	// the EM square is 1024 x 1024
			float	text_screen_height = base_matrix_max_scale
				* scale
				* 1024.0f
				/ 20.0f
				* pixel_scale;

			int	nominal_glyph_height = fnt->get_texture_glyph_nominal_size();
			float	max_glyph_height = fontlib::get_texture_glyph_max_height(fnt);
#ifdef GAMESWF_ALWAYS_USE_TEXTURES_FOR_TEXT_WHEN_POSSIBLE
			const bool	use_glyph_textures = true;
#else
			bool	use_glyph_textures =
				text_screen_height <= max_glyph_height * 1.0f;
#endif

			if (rec.m_style.m_has_x_offset)
			{
				x = rec.m_style.m_x_offset;
			}
			if (rec.m_style.m_has_y_offset)
			{
				y = rec.m_style.m_y_offset;
			}

			s_dummy_style[0].set_color(rec.m_style.m_color);

			rgba	transformed_color = cx.transform(rec.m_style.m_color);

			for (int j = 0; j < rec.m_glyphs.size(); j++)
			{
				int	index = rec.m_glyphs[j].m_glyph_index;
					
				mat = base_matrix;
				mat.concatenate_translation(x, y);
				mat.concatenate_scale(scale);

				if (index == -1)
				{
					// Invalid glyph; render it as an empty box.
					render::set_matrix(mat);
					render::line_style_color(transformed_color);

					// The EM square is 1024x1024, but usually isn't filled up.
					// We'll use about half the width, and around 3/4 the height.
					// Values adjusted by eye.
					// The Y baseline is at 0; negative Y is up.
					static const Sint16	s_empty_char_box[5 * 2] =
					{
						 32,   32,
						480,   32,
						480, -656,
						 32, -656,
						 32,   32
					};
					render::draw_line_strip(s_empty_char_box, 5);
				}
				else
				{
					const texture_glyph&	tg = fnt->get_texture_glyph(index);
					shape_character_def*	glyph = fnt->get_glyph(index);

					if (tg.is_renderable()
					    && (use_glyph_textures || glyph == NULL))
					{
						fontlib::draw_glyph(mat, tg, transformed_color, nominal_glyph_height);
					}
					else
					{

						// Draw the character using the filled outline.
						if (glyph)
						{
							glyph->display(mat, cx, pixel_scale, s_dummy_style, s_dummy_line_style);
						}
					}
				}
				x += rec.m_glyphs[j].m_glyph_advance;
			}
		}
	}


	struct text_character_def : public character_def
	{
		movie_definition_sub*	m_root_def;
		rect	m_rect;
		matrix	m_matrix;
		array<text_glyph_record>	m_text_glyph_records;

		// SwiffPlayer.dll 0x10086AB0. [CHOSEN, not recovered] S3 bridge:
		// the loader publishes the native resource ID separately.
		text_character_def(movie_definition_sub* root_def)
			:
			m_root_def(root_def)
		{
			assert(m_root_def);
		}

		// SwiffPlayer.dll 0x100875A0; deleting entry 0x10087570.
		// Array cleanup precedes the resource base.
		~text_character_def() = default;

		// SwiffPlayer.dll 0x10086B40.
		// [CHOSEN, not recovered] S3 bridge:
		// retained out-parameter adapter for the native text callers.
		rect* get_text_bounds(rect* out) const { *out = m_rect; return out; }
		// SwiffPlayer.dll 0x10086B40, exposed through native typed dispatch.
		rect get_bound_copy() const override { return m_rect; }

		// SwiffPlayer.dll 0x100867A0.
		void	read(stream* in, int tag_type, movie_definition_sub* m)
		{
			assert(m != NULL);
			assert(tag_type == 11 || tag_type == 33);

			m_rect.read(in);
			m_matrix.read(in);

			int	glyph_bits = in->read_u8();
			int	advance_bits = in->read_u8();

			IF_VERBOSE_PARSE(log_msg("begin text records\n"));

			text_style	style;
			for (;;)
			{
				int	first_byte = in->read_u8();
				
				if (first_byte == 0)
				{
					// This is the end of the text records.
					IF_VERBOSE_PARSE(log_msg("end text records\n"));
					break;
				}

				if ((first_byte & 0x80) != 0)
				{
					// This is a style change.

					bool	has_font = (first_byte >> 3) & 1;
					bool	has_color = (first_byte >> 2) & 1;
					bool	has_y_offset = (first_byte >> 1) & 1;
					bool	has_x_offset = (first_byte >> 0) & 1;

					IF_VERBOSE_PARSE(log_msg("  text style change\n"));

					if (has_font)
					{
						Uint16	font_id = in->read_u16();
						style.m_font_id = font_id;
						IF_VERBOSE_PARSE(log_msg("  has_font: font id = %d\n", font_id));
					}
					if (has_color)
					{
						if (tag_type == 11)
						{
							style.m_color.read_rgb(in);
						}
						else
						{
							assert(tag_type == 33);
							style.m_color.read_rgba(in);
						}
						IF_VERBOSE_PARSE(log_msg("  has_color\n"));
					}
					if (has_x_offset)
					{
						style.m_has_x_offset = true;
						style.m_x_offset = in->read_s16();
						IF_VERBOSE_PARSE(log_msg("  has_x_offset = %g\n", style.m_x_offset));
					}
					else
					{
						style.m_has_x_offset = false;
						style.m_x_offset = 0.0f;
					}
					if (has_y_offset)
					{
						style.m_has_y_offset = true;
						style.m_y_offset = in->read_s16();
						IF_VERBOSE_PARSE(log_msg("  has_y_offset = %g\n", style.m_y_offset));
					}
					else
					{
						style.m_has_y_offset = false;
						style.m_y_offset = 0.0f;
					}
					if (has_font)
					{
						style.m_text_height = in->read_u16();
						IF_VERBOSE_PARSE(log_msg("  text_height = %g\n", style.m_text_height));
					}
				}
				else
				{
					// Read the glyph record.

					int	glyph_count = first_byte & 0x7F;

					m_text_glyph_records.resize(m_text_glyph_records.size() + 1);
					m_text_glyph_records.back().m_style = style;
					m_text_glyph_records.back().read(in, glyph_count, glyph_bits, advance_bits);

					IF_VERBOSE_PARSE(log_msg("  glyph_records: count = %d\n", glyph_count));
				}
			}
		}


		// SwiffPlayer.dll 0x10086B70, definition table 0x100E1154 +0x10.
		// The shipped static-text definition has no drawing effects.
		void	display(character* /*inst*/)
		{
		}
	};


	// SwiffPlayer.dll 0x10086B80. [CHOSEN, not recovered] S3 bridge:
	// the native factory assigns the resource ID when publishing the definition.
	void	define_text_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Read a DefineText tag.
	{
		assert(tag_type == 11 || tag_type == 33);

		Uint16	character_id = in->read_u16();
		
		text_character_def*	ch = new text_character_def(m);
		IF_VERBOSE_PARSE(log_msg("text_character, id = %d\n", character_id));
		ch->read(in, tag_type, m);

		// IF_VERBOSE_PARSE(print some stuff);

		m->add_character(character_id, ch);
	}


	//
	// edit_text_character_def
	//


	struct edit_text_character_def : public character_def
	// A definition for a text display character, whose text can
	// be changed at runtime (by script or host).
	{
		movie_definition_sub*	m_root_def;
		rect			m_rect;
		tu_string		m_default_name;
		bool			m_word_wrap;
		bool			m_multiline;
		bool			m_password;	// show asterisks instead of actual characters
		bool			m_readonly;
		bool			m_auto_size;	// resize our bound to fit the text
		bool			m_no_select;
		bool			m_border;	// forces white background and black border -- silly, but sometimes used
		bool			m_html;

		// Allowed HTML (from Alexi's SWF Reference):
		//
		// <a href=url target=targ>...</a> -- hyperlink
		// <b>...</b> -- bold
		// <br> -- line break
		// <font face=name size=[+|-][0-9]+ color=#RRGGBB>...</font>  -- font change; size in TWIPS
		// <i>...</i> -- italic
		// <li>...</li> -- list item
		// <p>...</p> -- paragraph
		// <tab> -- insert tab
		// <TEXTFORMAT>  </TEXTFORMAT>
		//   [ BLOCKINDENT=[0-9]+ ]
		//   [ INDENT=[0-9]+ ]
		//   [ LEADING=[0-9]+ ]
		//   [ LEFTMARGIN=[0-9]+ ]
		//   [ RIGHTMARGIN=[0-9]+ ]
		//   [ TABSTOPS=[0-9]+{,[0-9]+} ]
		//
		// Change the different parameters as indicated. The
		// sizes are all in TWIPs. There can be multiple
		// positions for the tab stops. These are seperated by
		// commas.
		// <U>...</U> -- underline


		bool	m_use_outlines;	// when true, use specified SWF internal font.  Otherwise, renderer picks a default font

		int	m_font_id;
		font*	m_font;
		float	m_text_height;

		rgba	m_color;
		int	m_max_length;

		enum alignment
		{
			ALIGN_LEFT = 0,
			ALIGN_RIGHT,
			ALIGN_CENTER,
			ALIGN_JUSTIFY	// probably don't need to implement...
		};
		alignment	m_alignment;
		
		float	m_left_margin;	// extra space between box border and text
		float	m_right_margin;
		float	m_indent;	// how much to indent the first line of multiline text
		float	m_leading;	// extra space between lines (in addition to default font line spacing)
		tu_string	m_default_text;

		// SwiffPlayer.dll 0x10085300 initializes the flags, font/layout and colors.
		// [CHOSEN, not recovered] S3 bridge: the loader assigns the native resource
		// ID separately.
		edit_text_character_def(movie_definition_sub* root_def)
			:
			m_root_def(root_def),
			m_word_wrap(false),
			m_multiline(false),
			m_password(false),
			m_readonly(false),
			m_auto_size(false),
			m_no_select(false),
			m_border(false),
			m_html(false),
			m_use_outlines(false),
			m_font_id(-1),
			m_font(NULL),
			m_text_height(1.0f),
			m_max_length(1024),
			m_alignment(ALIGN_LEFT),
			m_left_margin(0.0f),
			m_right_margin(0.0f),
			m_indent(0.0f),
			m_leading(0.0f)
		{
			assert(m_root_def);

			m_color.set(0, 0, 0, 255);
		}

		// SwiffPlayer.dll 0x10085C70; deleting entry 0x10085C40.
		~edit_text_character_def()
		{
		}

		// SwiffPlayer.dll 0x100853E0.
		// [CHOSEN, not recovered] S3 bridge:
		// retained out-parameter adapter for the native text callers.
		rect* get_text_bounds(rect* out) const { *out = m_rect; return out; }
		// SwiffPlayer.dll 0x100853E0, exposed through native typed dispatch.
		rect get_bound_copy() const override { return m_rect; }

		// SwiffPlayer.dll 0x10085410 / 0x10085420.
		virtual float get_height_local() { return m_rect.height(); }
		virtual float get_width_local() { return m_rect.width(); }
		// SwiffPlayer.dll 0x10085430, forwarding to rect::point_test at 0x1009E320.
		virtual bool point_test_local(float x, float y) { return m_rect.point_test(x, y); }


		character*	create_character_instance(movie* parent, int id);


		// SwiffPlayer.dll 0x10085A00.
		void	read(stream* in, int tag_type, movie_definition_sub* m)
		{
			assert(m != NULL);
			assert(tag_type == 37);

			m_rect.read(in);

			in->align();
			bool	has_text = in->read_uint(1) ? true : false;
			m_word_wrap = in->read_uint(1) ? true : false;
			m_multiline = in->read_uint(1) ? true : false;
			m_password = in->read_uint(1) ? true : false;
			m_readonly = in->read_uint(1) ? true : false;
			bool	has_color = in->read_uint(1) ? true : false;
			bool	has_max_length = in->read_uint(1) ? true : false;
			bool	has_font = in->read_uint(1) ? true : false;

			in->read_uint(1);	// reserved
			m_auto_size = in->read_uint(1) ? true : false;
			bool	has_layout = in->read_uint(1) ? true : false;
			m_no_select = in->read_uint(1) ? true : false;
			m_border = in->read_uint(1) ? true : false;
			in->read_uint(1);	// reserved
			m_html = in->read_uint(1) ? true : false;
			m_use_outlines = in->read_uint(1) ? true : false;

			if (has_font)
			{
				m_font_id = in->read_u16();
				m_text_height = (float) in->read_u16();
			}

			if (has_color)
			{
				m_color.read_rgba(in);
			}

			if (has_max_length)
			{
				m_max_length = in->read_u16();
			}

			if (has_layout)
			{
				m_alignment = (alignment) in->read_u8();
				m_left_margin = (float) in->read_u16();
				m_right_margin = (float) in->read_u16();
				m_indent = (float) in->read_s16();
				m_leading = (float) in->read_s16();
			}

			char*	name = in->read_string();
			m_default_name = name;
			delete [] name;

			if (has_text)
			{
				char*	str = in->read_string();
				m_default_text = str;
				delete [] str;
			}

			IF_VERBOSE_PARSE(log_msg("edit_text_char, varname = %s, text = %s\n",
						 m_default_name.c_str(), m_default_text.c_str()));
		}
	};


	//
	// edit_text_character
	//


	// DICE's text field. The shipped SwiffPlayer replaced gameswf's
	// edit_text_character with a field that lays its text out through RENDERER-SIDE text-line objects
	// (render::text_* - one "font wrapper" per laid-out line, filled a character at a time so the
	// renderer can measure for word wrap), draws them at device size, and is the game's keyboard
	// input: focus, caret, selection, clipboard, Tab order, restrict/maxChars/password. Read from
	// SwiffPlayer.dll (2).i64: ctor sub_100875C0, formatText sub_10086080 / set_line sub_10085F50,
	// display sub_100882F0, mouse sub_10088650, key sub_10088ED0, insert sub_10088A70, delete
	// sub_10088DB0, focus sub_10087AC0, members sub_10089440 / sub_10087C60, notify paths
	// SwiffPlayer_notifyChar / setTextInput / getTextInput0/1. The stock glyph layout stays as the
	// fallback the shipped keeps (sub_10086C00) for a field whose font has no renderer name.
	struct edit_text_character;
	static edit_text_character*		s_focused_field = NULL;		// dword_100F8EE0
	// SwiffPlayer.dll 0x100D6C80 -> 0x10072300 destroys this borrowed-pointer array.
	static array<edit_text_character*>	s_tab_table;			// dword_100F8CB4 / dword_100F8CB8

	// SwiffPlayer.dll 0x10085950; the caller supplies a valid Tab index.
	static void register_text_tab(int index, edit_text_character* field)
	{
		if (s_tab_table.size() <= index)
		{
			s_tab_table.resize(index + 1);
		}
		s_tab_table[index] = field;
	}
	static tu_string			s_text_clipboard;			// `src`
	static bool	s_mod_shift = false, s_mod_alt = false, s_mod_ctrl = false, s_mod_caps = false;	// byte_100F8EE8..EB

	// Convert narrow text into a bounded UTF-16 buffer. Return the unit count
	// excluding a copied terminator, or size_t(-1) on decoding failure. Do not
	// repair or terminate the buffer after failure or capacity exhaustion.
	static size_t text_mbstowcs(Uint16* destination, const char* source, size_t capacity)
	{
		// MSVCR71 7.10.3052 mbstowcs 0x7C3550C3 -> 0x7C354FC8:
		// the C locale widens unsigned bytes, including bytes above 0x7F.
		const char* locale = std::setlocale(LC_CTYPE, NULL);
		if (strcmp(locale, "C") == 0 || strcmp(locale, "POSIX") == 0)
		{
			size_t count = 0;
			while (count < capacity)
			{
				destination[count] = static_cast<unsigned char>(source[count]);
				if (source[count] == 0) return count;
				++count;
			}
			return count;
		}
#if defined(_WIN32)
		static_assert(sizeof(wchar_t) == sizeof(Uint16), "Windows text uses UTF-16");
		return std::mbstowcs(reinterpret_cast<wchar_t*>(destination), source, capacity);
#else
		// [CHOSEN, not recovered] POSIX locale/wchar_t adapter for the macOS
		// port: native multibyte decoding, encoded as UTF-16. Its locale names,
		// code pages and error-prefix writes are not MSVCR71 emulation.
		std::mbstate_t state = {};
		size_t count = 0;
		while (count < capacity)
		{
			wchar_t character;
			const size_t bytes = std::mbrtowc(&character, source, MB_CUR_MAX, &state);
			if (bytes == static_cast<size_t>(-1) || bytes == static_cast<size_t>(-2))
			{
				errno = EILSEQ;
				return static_cast<size_t>(-1);
			}
			if (bytes == 0)
			{
				destination[count] = 0;
				return count;
			}
			const Uint32 codepoint = static_cast<Uint32>(character);
			if (codepoint > 0x10FFFFu)
			{
				errno = EILSEQ;
				return static_cast<size_t>(-1);
			}
			if (codepoint <= 0xFFFFu) destination[count++] = static_cast<Uint16>(codepoint);
			else
			{
				if (capacity - count < 2) return count;
				destination[count++] = static_cast<Uint16>(0xD800u + ((codepoint - 0x10000u) >> 10));
				destination[count++] = static_cast<Uint16>(0xDC00u + ((codepoint - 0x10000u) & 0x3FFu));
			}
			source += bytes;
		}
		return count;
#endif
	}

	// The digit-row keymap byte_100EC988 (three entries per key code 48..57: plain, shifted, AltGr).
	// A Swedish layout: shift-2 is ", shift-4 is ¤, AltGr-2 is @, AltGr-7 is {.
	static const unsigned char	s_digit_keymap[30] =
	{
		0x30, 0x3D, 0x7D,  0x31, 0x21, 0x00,  0x32, 0x22, 0x40,  0x33, 0x23, 0xA3,  0x34, 0x24, 0xA4,
		0x35, 0x25, 0x80,  0x36, 0x26, 0x00,  0x37, 0x2F, 0x7B,  0x38, 0x28, 0x5B,  0x39, 0x29, 0x5D,
	};

	struct edit_text_character : public character, public text_input
	{
		edit_text_character_def*	m_def;
		array<text_glyph_record>	m_text_glyph_records;
		array<fill_style>	m_dummy_style;	// used to pass a color on to shape_character::display()
		array<line_style>	m_dummy_line_style;
		tu_string	m_text;				// +300

		// DICE's members (shipped offsets in the comments).
		array<Uint16>	m_wide_text;	// +316 (a std::wstring): set through a wide String object; empty -> m_text
		bool	m_border;				// +264
		bool	m_background;			// +269
		rgba	m_border_color;			// +265 (black)
		rgba	m_background_color;		// +270 (white)
		rgba	m_text_color;			// +274
		int	m_mouse_state;				// +280: bit 0 = inside, bit 1 = button down
		int	m_prev_mouse_state;			// +284
		bool	m_first_mouse_frame;	// +288
		bool	m_dragging;				// +289
		float	m_drag_x, m_drag_y;		// +292 / +296 (twips, local)
		Sint16	m_caret_x, m_caret_y;	// +244 / +246: glyph-layout caret coordinates
		Uint16	m_caret;			// +248: character index, independent of renderer selection
		float	m_blink_time;			// +252: advance() accumulates, display blinks on (int)(t*3)&1, edits reset
		bool	m_lines_dirty;			// +344
		bool	m_password;				// +345
		int	m_tab_index;				// +348
		int	m_max_chars;				// +352
		//  +368 (0x170): the last height sub_10084DE0 was asked for
		// (0x10084E01 stores its int argument before the scale test), i.e. formatText's laid-out
		// height in pixels. `textHeight` answers it (vslot 0x10C = 0x10087A90, `mov eax,[ecx+170h]`);
		// the constructor seeds 666 (0x1008772C).
		int	m_text_height_px;
		tu_string	m_restrict;			// +356/+360
		as_value	m_on_changed;		// +376
		as_value	m_on_set_focus;		// +408
		as_environment	m_as_environment;	// +440
		// SwiffPlayer.dll 0x10089E16..0x10089E58 destroys the font name,
		// environment and focus handler in that order after releasing lines.
		tu_string	m_font_name;		// +8680
		array<void*>	m_lines;		// +8696/+8700: renderer text lines

		edit_text_character(movie* parent, edit_text_character_def* def, int id)
			:
			character(parent, id),
			m_def(def),
			m_border(false),
			m_background(false),
			m_border_color(0, 0, 0, 255),
			m_background_color(255, 255, 255, 255),
			m_text_color(0, 0, 0, 255),
			m_mouse_state(0),
			m_prev_mouse_state(0),
			m_first_mouse_frame(true),
			m_dragging(false),
			m_drag_x(0), m_drag_y(0),
			m_caret_x(0), m_caret_y(0), m_caret(0),
			m_blink_time(0),
			m_lines_dirty(false),
			m_password(false),
			m_tab_index(-1),
			m_max_chars(0),
			m_text_height_px(666)
		{
			// SwiffPlayer.dll 0x100875C0. Native character/Array/environment
			// ownership uses the S3 bridges declared on those types.
			assert(parent);
			assert(m_def);
			m_as_environment.set_target(this);

			// sub_100875C0: the def's border flag drives both border and background; colours black on
			// white; the text colour, password and maxChars from the def.
			m_border = m_def->m_border;
			m_background = m_def->m_border;
			m_text_color = m_def->m_color;
			// 0x10087815 appends the initial fill style before the parent Tab
			// query and renderer callbacks, even for empty text.
			m_dummy_style.push_back(fill_style());
			m_password = m_def->m_password;
			m_max_chars = m_def->m_max_length;

			// SwiffPlayer.dll 0x10087879 calls parent +0x70, the inherited Tab
			// index (sprite target 0x1007FB40), independently of placement ID.
			if (parent) set_tab_index(parent->get_tab_index());

			// 0x100878AC..0x100878F4 resolves the definition's font ID, calls
			// the renderer even for an empty exported name, and appends its
			// result even when null. [CHOSEN, not recovered] S3 bridge: the
			// definition's native dictionary replaces retail's current-root owner.
			if (m_def->m_root_def->get_export_name(m_def->m_font_id, &m_font_name))
			{
				const int height = bf2x87::toIntTruncate(bf2x87::mul(
					bf2x87::Real(m_def->m_text_height), bf2x87::Real(0.05f)));
				m_lines.push_back(render::text_create(m_font_name.c_str(), height));
			}

			set_text_value(m_def->m_default_text.c_str());
		}

		// SwiffPlayer.dll 0x10089D50; deleting wrapper 0x10089D10. The member
		// and base destructors supply the inspected release sequence below.
		// [CHOSEN, not recovered] S3 bridge: the separately owned arguments
		// Array can outlive this field, unlike retail's embedded Array. Native
		// mask/prototype ownership and issue #7 graph teardown remain in use.
		~edit_text_character()
		{
			// SwiffPlayer.dll 0x10089D50 removes Tab before renderer callbacks.
			// Members then release in declaration-reverse order; text_input
			// destruction clears focus before the character base is destroyed.
			if (m_tab_index >= 0 && m_tab_index < s_tab_table.size() && s_tab_table[m_tab_index] == this)
			{
				s_tab_table[m_tab_index] = NULL;
			}
			for (int i = 0; i < m_lines.size(); i++)
			{
				if (m_lines[i]) render::text_release(m_lines[i]);
			}
			m_lines.resize(0);
		}

		// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. Adds the
		// field's environment and its two handler values to the base character's values.
		virtual void	release_script_values(array<as_value>* held)
		{
			character::release_script_values(held);
			m_as_environment.release_values(held);
			hold_script_value(held, m_on_changed);
			m_on_changed.set_undefined();
			hold_script_value(held, m_on_set_focus);
			m_on_set_focus.set_undefined();
		}

		// SwiffPlayer.dll 0x100881A0: inspect members without stamping this field.
		virtual void gc_mark()
		{
			if (m_gc_generation == get_gc_mark_generation()) return;
			for (stringi_hash<as_value>::iterator it = m_as_environment.m_variables.begin();
				it != m_as_environment.m_variables.end(); ++it)
				gc_mark_value(it->second);
		}

		// SwiffPlayer.dll 0x10088250: release OBJECT members, then clear the bag.
		virtual void gc_release()
		{
			if (m_gc_generation == get_gc_release_generation()) return;
			for (stringi_hash<as_value>::iterator it = m_as_environment.m_variables.begin();
				it != m_as_environment.m_variables.end(); ++it)
			{
				if (it->second.get_type() == as_value::OBJECT)
				{
					as_object_interface* object = it->second.to_object_raw();
					if (object) object->gc_release();
				}
			}
			m_as_environment.m_variables.clear();
		}

		// SwiffPlayer.dll 0x10087B50. [CHOSEN, not recovered] S3 bridge:
		// retain the existing native pointer-array storage.
		void	set_tab_index(int index)
		{
			if (m_tab_index >= 0 && m_tab_index < s_tab_table.size() && s_tab_table[m_tab_index] == this)
			{
				s_tab_table[m_tab_index] = NULL;
			}
			m_tab_index = index;
			if (index >= 0)
			{
				register_text_tab(index, this);
			}
		}

		// TWIPS, the 2006 character convention: sprite_instance::get_width() derives a clip's width from
		// its children's, and the stock `_width` setter divides PIXELS_TO_TWIPS(value) by it - reporting
		// pixels here scaled every label clip (whose only child is its field) twenty-fold (retail run X,
		// 1640 px line widths, no wrapping). The shipped vslot 252 is in pixels; the AS-visible `_width`
		// stays in pixels through the conversions at the use sites below.
		// SwiffPlayer.dll 0x10087A60 / 0x10087A30.
		// [CHOSEN, not recovered] S3 bridge:
		// native extent virtuals return twips; their consumers convert to retail pixels.
		virtual float	get_width()  { rect bounds; return get_text_bounds(&bounds)->width(); }
		virtual float	get_height() { rect bounds; return get_text_bounds(&bounds)->height(); }
		// [CHOSEN, not recovered] S3 bridge from native twip-returning extent
		// virtuals to retail pixel consumers, without another float store.
		bf2x87::Real extent_pixels(bool width)
		{
			return bf2x87::mul(bf2x87::Real(width ? get_width() : get_height()), bf2x87::Real(0.05f));
		}
		//  the unscaled local extent Flash's `_width`/`_height` build on.
		virtual float	get_local_width()  { return m_def->m_rect.width(); }
		virtual float	get_local_height() { return m_def->m_rect.height(); }

		// SwiffPlayer.dll 0x10087C20.
		virtual const char*	get_text_name() const { return m_def->m_default_name.c_str(); }

		// SwiffPlayer.dll 0x10087970.
		// [CHOSEN, not recovered] S3 bridge:
		// retained out-parameter adapter for the native text callers.
		rect* get_text_bounds(rect* out) const { m_def->get_text_bounds(out); return out; }
		// SwiffPlayer.dll 0x10087970 forwards the definition rectangle.
		rect get_bound_copy() const override { return m_def->get_bound_copy(); }

		// SwiffPlayer.dll 0x10087990.
		bool point_test_text_local(float x, float y) const
		{
			rect bounds;
			return get_text_bounds(&bounds)->point_test(x, y);
		}

		// SwiffPlayer.dll 0x10087A90.
		virtual int get_text_height() const { return m_text_height_px; }


		// SwiffPlayer.dll 0x10087510.
		virtual void	set_text_value(const char* new_text)
		// Set our text to the given string. sub_10087510: same text -> nothing; the field's own
		// maxChars clamps; then the layout. The WIDE store is NOT touched: the two stores are
		// independent in the shipped field (this setter writes +300, the wide one +316), and the
		// line layout prefers the wide one - which is how a label survives the components'
		// `arrange` writing the localisation KEY over a translation that arrived as a String
		// object (retail run W showed the keys when this cleared m_wide_text).
		{
			if (m_text == new_text)
			{
				return;
			}
			m_text = new_text;
			if (m_max_chars > 0 && m_text.length() > m_max_chars)
			{
				m_text.resize(m_max_chars);
			}
			format_text();
		}

		// The wide setter (shipped vslot +272, sub_100893E0): a String object carrying UTF-16 text.
		// Same text -> nothing; else the wide store alone is replaced (the narrow one at +300 is NOT
		// synced), clamped to maxChars, and the layout runs.
		// SwiffPlayer.dll 0x100893E0. [CHOSEN, not recovered] S3 bridge:
		// store UTF-16 in the existing array, including its terminator.
		void	set_wide_text_value(const Uint16* wide)
		{
			int	n = 0;
			for (const Uint16* p = wide; p && *p; ++p) n++;
			if (m_wide_text.size() == n + 1 || (n == 0 && m_wide_text.size() == 0))
			{
				bool	same = true;
				for (int i = 0; i < n; i++) { if (m_wide_text[i] != wide[i]) { same = false; break; } }
				if (same) return;
			}
			m_wide_text.resize(0);
			for (int i = 0; i < n; i++)
			{
				m_wide_text.push_back(wide[i]);
			}
			if (m_max_chars > 0 && m_wide_text.size() > m_max_chars)
			{
				m_wide_text.resize(m_max_chars);
			}
			m_wide_text.push_back(0);
			format_text();
		}

		// SwiffPlayer.dll 0x10087C40.
		virtual const char*	get_text_value() const
		{
			return m_text.c_str();
		}


		void	set_member(const tu_stringi& name, const as_value& val)
		// We have a "text" member.
		{
			// @@ TODO need to inherit basic stuff like _x, _y, _xscale, _yscale etc

			if (dice_set_member(name, val))		//  the shipped member surface, sub_10089440
			{
				return;
			}
			as_standard_member	std_member = get_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_TEXT:
				//if (name == "text")
			{
				int version = get_parent()->get_movie_definition()->get_version();
				set_text_value(val.to_tu_string_versioned(version));
				return;
			}
			case M_X:
				//else if (name == "_x")
			{
				matrix	m = get_matrix();
				m.m_[0][2] = (float) PIXELS_TO_TWIPS(val.to_number());
				set_matrix(m);

				// m_accept_anim_moves = false;
				
				return;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();
				m.m_[1][2] = (float) PIXELS_TO_TWIPS(val.to_number());
				set_matrix(m);

				// m_accept_anim_moves = false;
				
				return;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				set_visible(val.to_bool());
				return;
			}
			case M_ALPHA:
				//else if (name == "_alpha")
			{
				// @@ TODO this should be generic to struct character!
				// Arg is in percent.
				cxform	cx = get_cxform();
				cx.m_[3][0] = fclamp(float(val.to_number()) / 100.f, 0, 1);
				set_cxform(cx);
				return;
			}
			case M_TEXTCOLOR:
				//else if (name == "textColor")
			{	
				// The arg is 0xRRGGBB format.
#if defined(__APPLE__)
				// [CHOSEN, not recovered] Keep the x86 wrap of a negative number (arm64 saturates to 0).
				Uint32	rgb = bf2x87::lowUInt32(val.to_number());
#else
				Uint32	rgb = (Uint32) val.to_number();
#endif

				cxform	cx = get_cxform();
				cx.m_[0][0] = fclamp(((rgb >> 16) & 255) / 255.0f, 0, 1);
				cx.m_[1][0] = fclamp(((rgb >>  8) & 255) / 255.0f, 0, 1);
				cx.m_[2][0] = fclamp(((rgb      ) & 255) / 255.0f, 0, 1);
				set_cxform(cx);

				return;
			}
			// @@ TODO see TextField members in Flash MX docs
			}	// end switch
		}


		bool	get_member(const tu_stringi& name, as_value* val)
		{
			if (dice_get_member(name, val))		//  the shipped member surface, sub_10087C60
			{
				return true;
			}
			// SwiffPlayer.dll 0x10087C60 compares member names case-sensitively.
			// Route the four remaining shared properties through the native switch.
			const tu_string& exact_name = name.to_tu_string();
			if (exact_name != "_x" && exact_name != "_y" && exact_name != "_visible"
			    && exact_name != "_alpha") return false;
			as_standard_member	std_member = get_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_TEXT:
				//if (name == "text")
			{
				val->set_tu_string(m_text);
				return true;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				val->set_bool(get_visible());
				return true;
			}
			case M_ALPHA:
				//else if (name == "_alpha")
			{
				// @@ TODO this should be generic to struct character!
				const cxform&	cx = get_cxform();
				val->set_double(cx.m_[3][0] * 100.f);
				return true;
			}
			case M_TEXTCOLOR:
				//else if (name == "textColor")
			{
				// Return color in 0xRRGGBB format
				const cxform&	cx = get_cxform();
				int	r = iclamp(int(cx.m_[0][0] * 255), 0, 255);
				int	g = iclamp(int(cx.m_[0][0] * 255), 0, 255);
				int	b = iclamp(int(cx.m_[0][0] * 255), 0, 255);
				val->set_int((r << 16) + (g << 8) + b);
				return true;
			}
			case M_X:
				//else if (name == "_x")
			{
				matrix	m = get_matrix();	// @@ get_world_matrix()???
				val->set_double(TWIPS_TO_PIXELS(m.m_[0][2]));
				return true;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();	// @@ get_world_matrix()???
				val->set_double(TWIPS_TO_PIXELS(m.m_[1][2]));
				return true;
			}
			case M_WIDTH:
				//else if (name == "_width")
			{
				// @@ TODO should implement this in
				// character and inherit into both here and sprite_instance
				rect	transformed_rect;
				transformed_rect.enclose_transformed_rect(get_world_matrix(), m_def->m_rect);
				val->set_double(TWIPS_TO_PIXELS(transformed_rect.width()));
				return true;
			}
			case M_HEIGHT:
				//else if (name == "_height")
			{
				// @@ TODO should implement this in
				// character and inherit into both here and sprite_instance
				rect	transformed_rect;
				transformed_rect.enclose_transformed_rect(get_world_matrix(), m_def->m_rect);
				val->set_double(TWIPS_TO_PIXELS(transformed_rect.height()));
				return true;
			}
			}	// end switch

			return false;
		}

		
		// SwiffPlayer.dll 0x10085440. [CHOSEN, not recovered] S3 bridge:
		// get_width already returns twips in the current character interface.
		void	align_line(edit_text_character_def::alignment align, int last_line_start_record, float x)
		// Does LEFT/CENTER/RIGHT alignment on the records in
		// m_text_glyph_records[], starting with
		// last_line_start_record and going through the end of
		// m_text_glyph_records.
		{
			float	extra_space = (get_width() - m_def->m_right_margin) - x;

			float	shift_right = 0.0f;

			if (align == edit_text_character_def::ALIGN_LEFT)
			{
				// Nothing to do; already aligned left.
				return;
			}
			else if (align == edit_text_character_def::ALIGN_CENTER)
			{
				// Distribute the space evenly on both sides.
				shift_right = extra_space / 2;
			}
			else if (align == edit_text_character_def::ALIGN_RIGHT)
			{
				// Shift all the way to the right.
				shift_right = extra_space;
			}

			// Shift the beginnings of the records on this line.
			for (int i = last_line_start_record; i < m_text_glyph_records.size(); i++)
			{
				text_glyph_record&	rec = m_text_glyph_records[i];

				if (rec.m_style.m_has_x_offset)
				{
					rec.m_style.m_x_offset += shift_right;
				}
			}
		}

		// Convert the characters in m_text into a series of
		// text_glyph_records to be rendered.
		//  sub_10086C00: with renderer lines the renderer lays the text out, otherwise the
		// stock glyph layout below runs (the shipped keeps both).
		void	format_text()
		{
			if (m_lines.size() > 0)
			{
				format_text_lines();
				return;
			}
			format_text_glyphs();
		}

		// SwiffPlayer.dll 0x10085F50. [CHOSEN, not recovered] S3 bridge:
		// keep UTF-16 in the native array and widen only at the renderer boundary.
		void	set_line_text(int index, float width_px, const array<Uint16>& text)
		{
			if (index >= m_lines.size())
			{
				void*	line = render::text_create(m_font_name.c_str(), (int) (m_def->m_text_height * 0.05f));
				m_lines.push_back(line);
			}
			void*	line = m_lines[index];
			render::text_set_size(line, width_px, (int) m_def->m_alignment);
			if (!m_password)
			{
				render::text_set(line, BF2_DICE_WIDE(&text[0]));	// [CHOSEN, not recovered] POSIX: widen, see dice_wide
				return;
			}
			// A password field shows one asterisk per character (the shipped fills a 512-wide buffer).
			static Uint16	s_asterisks[512];
			if (s_asterisks[0] == 0)
			{
				for (int i = 0; i < 511; i++) s_asterisks[i] = '*';
				s_asterisks[511] = 0;
			}
			int	n = 0;
			while (text[n]) n++;
			if (n > 511) n = 511;
			render::text_set(line, BF2_DICE_WIDE(&s_asterisks[511 - n]));
		}

		// sub_10084DE0: grow the field's y scale so at least `height_px` is visible (auto-size).
		// SwiffPlayer.dll 0x10084DE0. [CHOSEN, not recovered] S3 bridge:
		// the current character extent API returns twips rather than retail pixels.
		void	set_height_at_least(int height_px)
		{
			m_text_height_px = height_px;
			matrix	m = get_matrix();
			if (TWIPS_TO_PIXELS(get_height()) * m.m_[1][1] < height_px)
			{
				m.m_[1][1] = height_px / TWIPS_TO_PIXELS(get_height());
				set_matrix(m);
			}
		}

		// SwiffPlayer.dll 0x10086080.
		// [CHOSEN, not recovered] S3 bridge: native UTF-16 storage. Non-C
		// POSIX conversion follows the explicit native-locale adapter above.
		void	format_text_lines()
		{
			static profile_record counter("formatTextObjects");
			if (s_profiling) counter.begin();
			// Borrow the wide payload, or allocate the narrow byte count including
			// its terminator. Retail ignores the conversion result and does not
			// initialize the remaining buffer after conversion failure.
			Uint16* converted = NULL;
			size_t converted_bytes = 0;
			const Uint16* text;
			if (m_wide_text.size() > 1)
			{
				text = &m_wide_text[0];
			}
			else
			{
				const size_t count = static_cast<size_t>(m_text.length()) + 1;
				converted_bytes = count * sizeof(Uint16);
				converted = static_cast<Uint16*>(tu_malloc(converted_bytes));
				text_mbstowcs(converted, m_text.c_str(), count);
				text = converted;
			}

			const matrix	world = get_world_matrix();
			const float width_px = bf2x87::store(bf2x87::mul(extent_pixels(true), bf2x87::Real(world.m_[0][0])));
			const float height_limit = bf2x87::store(bf2x87::mul(
				bf2x87::mul(extent_pixels(false), bf2x87::Real(20.0f)), bf2x87::Real(world.m_[1][1])));

			array<Uint16>	buf;
			buf.push_back(0);
			float	y = 0.0f;
			int	line = 0;
			int	last_space = -1;
			bool	overflowed = false;
			int	i = 0;
			Uint16 c;
			for (;;)
			{
				c = text[i++];
				if (bf2x87::greater(bf2x87::Real(y), bf2x87::Real(height_limit)))
				{
					overflowed = true;
					break;
				}
				if (c == 13 || c == 10)
				{
					set_line_text(line++, width_px, buf);
					buf.resize(0);
					buf.push_back(0);
					y = bf2x87::store(bf2x87::add(bf2x87::add(bf2x87::Real(m_def->m_leading),
						bf2x87::Real(m_def->m_text_height)), bf2x87::Real(y)));
				}
				else
				{
					if (c == 32)
					{
						last_space = buf.size() - 1;
					}
					buf[buf.size() - 1] = c;
					buf.push_back(0);
					set_line_text(line, width_px, buf);
					if (m_def->m_word_wrap)
					{
						float	w = 0, h = 0;
						render::text_extent(m_lines[line], buf.size() - 1, &w, &h);
						const bf2x87::Real wrap_limit = bf2x87::sub(bf2x87::Real(width_px),
							bf2x87::mul(bf2x87::Real(m_def->m_right_margin), bf2x87::Real(0.05f)));
						if (!bf2x87::lessEqual(wrap_limit, bf2x87::Real(w)))
						{
							// fits
						}
						else if (last_space == -1)
						{
							// No space to break at: the last character opens the next line.
							buf.resize(buf.size() - 2);
							buf.push_back(0);
							set_line_text(line++, width_px, buf);
							buf.resize(0);
							buf.push_back(c);
							buf.push_back(0);
							y = bf2x87::store(bf2x87::add(bf2x87::add(bf2x87::Real(m_def->m_leading),
								bf2x87::Real(m_def->m_text_height)), bf2x87::Real(y)));
						}
						else
						{
							// Break at the last space: the tail after it moves to the next line.
							array<Uint16>	tail;
							for (int k = last_space + 1; k < buf.size(); k++) tail.push_back(buf[k]);
							buf.resize(last_space);
							buf.push_back(0);
							set_line_text(line++, width_px, buf);
							buf = tail;
							last_space = -1;
							y = bf2x87::store(bf2x87::add(bf2x87::add(bf2x87::Real(m_def->m_leading),
								bf2x87::Real(m_def->m_text_height)), bf2x87::Real(y)));
						}
					}
				}
				if (c == 0)
				{
					break;
				}
			}
			if (overflowed)
			{
				line--;
			}
			if ((!overflowed || c == 0) && buf.size() <= 1)
			{
				line--;
			}
			if (converted) tu_free(converted, converted_bytes);

			const int	count = m_lines.size();
			if (line >= count && line > 0)
			{
				line = count - 1;
			}
			for (int k = 0; k < m_lines.size(); k++)
			{
				render::text_caret(m_lines[k], m_caret);
			}
			for (int k = line + 1; k < m_lines.size(); k++)
			{
				render::text_release(m_lines[k]);
			}
			if (line + 1 != m_lines.size() && line + 1 >= 0)
			{
				m_lines.resize(line + 1);
			}

			//  sub_10086080's tail (0x10086433..0x10086495): the height the
			// field must show is the font's LEADING (+0x40, the same term as the caret's, #47) plus the
			// definition's text height, plus the laid-out y for word-wrapped fields or the definition's
			// leading otherwise, in pixels. Until 2026-09-13 this port added the font's ASCENT instead,
			// which grew every input field's y scale three to four times: the caret and text sat low
			// in the box and the field's hit rectangle reached two rows down, so a click on the e-mail
			// field still belonged to ACCOUNT NAME (owner's report, the CREATE ACCOUNT page).
			const float	leading = m_def->m_font->get_leading();
			bf2x87::Real height;
			if (m_def->m_word_wrap)
			{
				height = bf2x87::add(bf2x87::add(bf2x87::Real(leading), bf2x87::Real(m_def->m_text_height)), bf2x87::Real(y));
			}
			else
			{
				height = bf2x87::add(bf2x87::add(bf2x87::Real(leading), bf2x87::Real(m_def->m_leading)), bf2x87::Real(m_def->m_text_height));
			}
			set_height_at_least(bf2x87::toIntTruncate(bf2x87::mul(height, bf2x87::Real(0.05f))));
			m_lines_dirty = true;
			counter.end();
		}

		// SwiffPlayer.dll 0x10086C00, glyph fallback arm.
		void	format_text_glyphs()
		{
			m_text_glyph_records.resize(0);

			if (m_def->m_font == NULL)
			{
				return;
			}
			static profile_record counter("formatText");
			if (s_profiling) counter.begin();

			// @@ mostly for debugging
			// Font substitution -- if the font has no
			// glyphs, try some other defined font!
			if (m_def->m_font->get_glyph_count() == 0)
			{
				// Find a better font.
				font*	newfont = m_def->m_font;
				for (int i = 0, n = fontlib::get_font_count(); i < n; i++)
				{
					font*	f = fontlib::get_font(i);
					assert(f);

					if (f->get_glyph_count() > 0)
					{
						// This one looks good.
						newfont = f;
						break;
					}
				}

				if (m_def->m_font != newfont)
				{
					log_error("error: substituting font!  font '%s' has no glyphs, using font '%s'\n",
						  fontlib::get_font_name(m_def->m_font),
						  fontlib::get_font_name(newfont));

					m_def->m_font = newfont;
				}
			}


			float scale = bf2x87::store(bf2x87::mul(bf2x87::Real(m_def->m_text_height), bf2x87::Real(1.0f / 1024.0f)));

			text_glyph_record	rec;	// one to work on
			rec.m_style.m_font = m_def->m_font;
			rec.m_style.m_color = m_text_color;
			const bf2x87::Real indent = bf2x87::add(bf2x87::Real(m_def->m_indent), bf2x87::Real(m_def->m_left_margin));
			rec.m_style.m_x_offset = bf2x87::greater(indent, bf2x87::Real(0)) ? bf2x87::store(indent) : 0.0f;
			rec.m_style.m_y_offset = bf2x87::store(bf2x87::add(
				bf2x87::mul(bf2x87::sub(bf2x87::Real(m_def->m_font->get_leading()),
					bf2x87::Real(m_def->m_font->get_descent())), bf2x87::Real(scale)), bf2x87::Real(m_def->m_text_height)));
			rec.m_style.m_text_height = m_def->m_text_height;
			rec.m_style.m_has_x_offset = true;
			rec.m_style.m_has_y_offset = true;
			rect bounds;
			get_text_bounds(&bounds);
			rec.m_style.m_x_offset += bounds.get_corner(0).m_x;
			rec.m_style.m_y_offset += bounds.get_corner(0).m_y;

			float	x = rec.m_style.m_x_offset;
			float	y = rec.m_style.m_y_offset;
			m_caret_x = text_coord(x);
			m_caret_y = text_coord(bf2x87::add(bf2x87::add(bf2x87::mul(bf2x87::Real(scale),
				bf2x87::Real(m_def->m_font->get_descent())), bf2x87::Real(y)), bf2x87::Real(40.0f)));

			float leading = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(scale),
				bf2x87::Real(m_def->m_font->get_leading())), bf2x87::Real(m_def->m_leading)));

			int	last_code = -1;
			int	last_space_glyph = -1;
			int	last_line_start_record = 0;

			const char*	text_start = m_text.c_str();
			const char*	text = text_start;
			while (Uint32 code = utf8::decode_next_unicode_character(&text))
			{
				// 0x10086E33..0x10086E60 clips the unfinished line at the height limit.
				const float baseline = bf2x87::store(bf2x87::add(bf2x87::mul(
					bf2x87::Real(m_def->m_font->get_descent()), bf2x87::Real(scale)), bf2x87::Real(y)));
				if (bf2x87::less(bf2x87::mul(extent_pixels(false), bf2x87::Real(20.0f)), bf2x87::Real(baseline)))
				{
					rec.m_glyphs.resize(0);
					break;
				}

				x = bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(m_def->m_font->get_kerning_adjustment(last_code, (int) code)),
					bf2x87::Real(scale)), bf2x87::Real(x)));
				last_code = (int) code;


				if (code == 13 || code == 10)
				{
					// newline.

					// Frigging Flash seems to use '\r' (13) as its
					// default newline character.  If we get DOS-style \r\n
					// sequences, it'll show up as double newlines, so maybe we
					// need to detect \r\n and treat it as one newline.

					// Close out this stretch of glyphs.
					m_text_glyph_records.push_back(rec);
					align_line(m_def->m_alignment, last_line_start_record, x);

					const bf2x87::Real paragraph_indent = bf2x87::add(bf2x87::Real(m_def->m_indent), bf2x87::Real(m_def->m_left_margin));
					x = bf2x87::greater(paragraph_indent, bf2x87::Real(0)) ? bf2x87::store(paragraph_indent) : 0.0f;
					y = bf2x87::store(bf2x87::add(bf2x87::add(bf2x87::Real(leading), bf2x87::Real(m_def->m_text_height)), bf2x87::Real(y)));

					// Start a new record on the next line.
					rec.m_glyphs.resize(0);
					rec.m_style.m_font = m_def->m_font;
					rec.m_style.m_color = m_def->m_color;
					rec.m_style.m_x_offset = x;
					rec.m_style.m_y_offset = y;
					rec.m_style.m_text_height = m_def->m_text_height;
					rec.m_style.m_has_x_offset = true;
					rec.m_style.m_has_y_offset = true;

					last_space_glyph = -1;
					last_line_start_record = m_text_glyph_records.size();

					continue;
				}

				// Remember where word breaks occur.
				if (code == 32)
				{
					last_space_glyph = rec.m_glyphs.size();
				}

				int	index = m_def->m_font->get_glyph_index((Uint16) code);
				if (index == -1)
				{
					// error -- missing glyph!
					
					// Log an error, but don't log too many times.
					static int	s_log_count = 0;
					if (s_log_count < 10)
					{
						s_log_count++;
						log_error("edit_text_character::display() -- missing glyph for char %d "
							  "-- make sure character shapes for font %s are being exported "
							  "into your SWF file!\n",
							  code,
							  m_def->m_font->get_name());
					}

					// SwiffPlayer.dll 0x10086EB4..0x10086EE5 skips missing glyphs.
					continue;
				}
				text_glyph_record::glyph_entry	ge;
				ge.m_glyph_index = index;
				ge.m_glyph_advance = scale * m_def->m_font->get_advance(index);

				rec.m_glyphs.push_back(ge);

				x += ge.m_glyph_advance;

				
				// SwiffPlayer.dll 0x10086F2B..0x10086F4C.
				if (bf2x87::lessEqual(bf2x87::sub(bf2x87::mul(extent_pixels(true), bf2x87::Real(20.0f)),
					bf2x87::Real(m_def->m_right_margin)), bf2x87::Real(x)))
				{
					// Whoops, we just exceeded the box width.  Do word-wrap.

					// Insert newline.

					// Close out this stretch of glyphs.
					m_text_glyph_records.push_back(rec);
					float	previous_x = x;

					x = m_def->m_left_margin;
					y = bf2x87::store(bf2x87::add(bf2x87::add(bf2x87::Real(leading), bf2x87::Real(m_def->m_text_height)), bf2x87::Real(y)));

					// Start a new record on the next line.
					rec.m_glyphs.resize(0);
					rec.m_style.m_font = m_def->m_font;
					rec.m_style.m_color = m_def->m_color;
					rec.m_style.m_x_offset = x;
					rec.m_style.m_y_offset = y;
					rec.m_style.m_text_height = m_def->m_text_height;
					rec.m_style.m_has_x_offset = true;
					rec.m_style.m_has_y_offset = true;
					
					text_glyph_record&	last_line = m_text_glyph_records.back();
					if (!m_def->m_word_wrap)
					{
						// 0x10087207: discard overflow and scan through the next newline.
						if (last_line.m_glyphs.size() > 0)
						{
							last_line.m_glyphs.resize(last_line.m_glyphs.size() - 1);
						}
						do { code = utf8::decode_next_unicode_character(&text); }
						while (code != 0 && code != 13 && code != 10);
						if (code == 0) break;
					}
					else if (last_space_glyph == -1)
					{
						// Pull the previous glyph down onto the
						// new line.
						if (last_line.m_glyphs.size() > 0)
						{
							if ((Uint16) (text - text_start - 1) == m_caret)
							{
								set_glyph_caret(x, y, scale, (int) code, (int) code);
							}
							rec.m_glyphs.push_back(last_line.m_glyphs.back());
							x += last_line.m_glyphs.back().m_glyph_advance;
							previous_x -= last_line.m_glyphs.back().m_glyph_advance;
							last_line.m_glyphs.resize(last_line.m_glyphs.size() - 1);
						}
					}
					else
					{
						// The removed space contributes to the old line's aligned width.
						previous_x -= last_line.m_glyphs[last_space_glyph].m_glyph_advance;
						const int byte_start = (int) (text - text_start) - last_line.m_glyphs.size();
						if (byte_start + last_space_glyph + 1 == m_caret)
						{
							set_glyph_caret(x, y, scale, (int) code, (int) code);
						}
						// Move the previous word down onto the next line.

						for (int i = last_space_glyph + 1; i < last_line.m_glyphs.size(); i++)
						{
							rec.m_glyphs.push_back(last_line.m_glyphs[i]);
							x += last_line.m_glyphs[i].m_glyph_advance;
							previous_x -= last_line.m_glyphs[i].m_glyph_advance;
							if (byte_start + i + 1 == m_caret)
							{
								set_glyph_caret(x, y, scale, (int) code, (int) code);
							}
						}
						last_line.m_glyphs.resize(last_space_glyph);
					}

					align_line(m_def->m_alignment, last_line_start_record, previous_x);

					last_space_glyph = -1;
					last_line_start_record = m_text_glyph_records.size();
				}
				if ((Uint16) (text - text_start) == m_caret)
				{
					set_glyph_caret(x, y, scale, last_code, (int) code);
				}

				// TODO: HTML markup
			}

			// Add this line to our output.
			m_text_glyph_records.push_back(rec);
			align_line(m_def->m_alignment, last_line_start_record, x);
			counter.end();
		}

		// SwiffPlayer.dll 0x100872A6..0x100872E8 and the wrap branches.
		void set_glyph_caret(float x, float y, float scale, int previous, int code)
		{
			m_caret_x = text_coord(bf2x87::add(bf2x87::mul(bf2x87::Real(m_def->m_font->get_kerning_adjustment(previous, code)),
				bf2x87::Real(scale)), bf2x87::Real(x)));
			m_caret_y = text_coord(bf2x87::add(bf2x87::add(bf2x87::mul(bf2x87::Real(scale),
				bf2x87::Real(m_def->m_font->get_descent())), bf2x87::Real(y)), bf2x87::Real(40.0f)));
		}


		// SwiffPlayer.dll 0x100882F0.
		// [CHOSEN, not recovered] S3 bridge:
		// read the definition rectangle through the current native field.
		//  display: refresh dirty lines, the world matrix with its SCALE
		// reset to one (text is drawn at device size, translation only), background quad and border,
		// the blinking caret when focused, then every line through render::text_draw with the y
		// advanced by leading + height. The stock glyph records are never drawn (the shipped does not).
		void	display()
		{
			if (m_lines_dirty)
			{
				for (int i = 0; i < m_lines.size(); i++)
				{
					render::text_detach(m_lines[i]);
				}
				m_lines_dirty = false;
			}

			matrix	mat = get_world_matrix();
			mat.m_[0][0] = 1.0f;
			mat.m_[1][1] = 1.0f;
			render::set_matrix(mat);
			render::set_cxform(get_world_cxform());

			point	coords[4];
			rect bounds;
			get_text_bounds(&bounds);
			coords[0] = bounds.get_corner(0);
			coords[1] = bounds.get_corner(1);
			coords[2] = bounds.get_corner(3);
			coords[3] = bounds.get_corner(2);
			Sint16	icoords[18] =
			{
				// strip (fill in)
				text_coord(coords[0].m_x), text_coord(coords[0].m_y),
				text_coord(coords[1].m_x), text_coord(coords[1].m_y),
				text_coord(coords[2].m_x), text_coord(coords[2].m_y),
				text_coord(coords[3].m_x), text_coord(coords[3].m_y),
				// outline
				text_coord(coords[0].m_x), text_coord(coords[0].m_y),
				text_coord(coords[1].m_x), text_coord(coords[1].m_y),
				text_coord(coords[3].m_x), text_coord(coords[3].m_y),
				text_coord(coords[2].m_x), text_coord(coords[2].m_y),
				text_coord(coords[0].m_x), text_coord(coords[0].m_y),
			};
			if (m_background)
			{
				render::fill_style_color(0, m_background_color);
				render::draw_mesh_strip(&icoords[0], 4);
			}
			if (m_border)
			{
				render::line_style_color(m_border_color);
				render::line_style_width(1.0f);
				render::draw_line_strip(&icoords[8], 5);
			}

			if (m_lines.size() > 0)
			{
				const float	em = m_def->m_text_height * (1.0f / 1024.0f);
				if (s_focused_field == this)
				{
					if ((bf2x87::toIntTruncate(m_blink_time * 3.0f) & 1) == 0)
					{
						float	w = 0, h = 0;
						render::text_extent(m_lines[0], m_caret, &w, &h);
						const float	x = w * 20.0f;
						//  The font term is the font's LEADING, not its
						// ascent. 100885 8B..9E reads def+0x44 (the font) and multiplies em by
						// FONT+0x40, and font::read fills +0x38/+0x3C/+0x40 with ascent/descent/
						// leading in that order (100A1A7D..100A1AAB), so +0x40 is the third of the
						// three. sub_10086C00's own baseline, (font+0x40 - font+0x3C) * em +
						// text_height, is the stock (leading - descent) line, which pins the same
						// mapping from the other side. Measured: the shipped DLL draws an 11 px
						// caret in the login password field where the ascent reading drew 18.
						// The shipped body does not null-test the font here, and neither does this.
						const float	fontLeading = m_def->m_font->get_leading();
						Sint16	caret[4] =
						{
							text_coord(x), 0,
							text_coord(x), text_coord(em * fontLeading + m_def->m_leading + m_def->m_text_height),
						};
						render::line_style_color(m_text_color);
						render::line_style_width(1.0f);
						render::draw_line_strip(caret, 2);
					}
				}

				float	left = m_def->m_indent + m_def->m_left_margin;
				if (!(left > 0)) left = 0;
				mat.m_[0][2] += left + 40.0f;
				mat.m_[1][2] += 30.0f;
				for (int i = 0; i < m_lines.size(); i++)
				{
					render::text_draw(m_lines[i], &mat, pack_color(m_text_color), get_depth());
					mat.m_[1][2] += m_def->m_leading + m_def->m_text_height;
				}
			}

			// SwiffPlayer.dll 0x10088649 returns after drawing the last renderer line.
		}

		// SwiffPlayer.dll 0x100883F5..0x10088463: _ftol then the low word.
		static Sint16 text_coord(bf2x87::Real value)
		{
			return bf2x87::lowShort(bf2x87::toInt64Truncate(value));
		}

		// SwiffPlayer.dll 0x10087AA0.
		virtual void	advance(float delta_time)
		{
			m_blink_time += delta_time;
		}

		// The four colour bytes as the one dword the shipped passes (memory order r,g,b,a).
		static unsigned	pack_color(const rgba& c)
		{
			return (unsigned) c.m_r | ((unsigned) c.m_g << 8) | ((unsigned) c.m_b << 16) | ((unsigned) c.m_a << 24);
		}

		void	call_callback(as_value& fn)	// sub_1008EAA0: the field's AS object is `this`
		{
			// SwiffPlayer.dll 0x10087B01 / 0x10088E6E tests only undefined.
			// [CHOSEN, not recovered] S3 bridge to the native call wrapper.
			if (fn.get_type() != as_value::UNDEFINED)
			{
				call_method0(fn, &m_as_environment, this);
			}
		}

		// SwiffPlayer.dll 0x10087AC0. [CHOSEN, not recovered] S3 bridge:
		// tolerate the current factory's absence of renderer lines.
		void	focus()
		{
			s_focused_field = this;
			m_caret = (Uint16) m_text.length();
			if (m_lines.size() > 0)
			{
				render::text_caret(m_lines[0], m_caret);
			}
			m_blink_time = 0;
			call_callback(m_on_set_focus);
		}

		// SwiffPlayer.dll 0x100891E0; from is the registered Tab index.
		static bool	focus_next(int from)
		{
			const int	n = s_tab_table.size();
			for (int i = from + 1; i < n; i++)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			for (int i = 0; i < from; i++)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			return false;
		}
		// SwiffPlayer.dll 0x10089230; from is the registered Tab index.
		static bool	focus_prev(int from)
		{
			const int	n = s_tab_table.size();
			for (int i = from - 1; i >= 0; i--)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			for (int i = n - 1; i > from; i--)
			{
				if (s_tab_table[i]) { s_tab_table[i]->focus(); return true; }
			}
			return false;
		}

		// SwiffPlayer.dll 0x100879C0: inverse world transform, then the local rectangle.
		virtual bool	point_test_world(float x, float y)
		{
			matrix	world = get_world_matrix();
			point	p;
			world.transform_by_inverse(&p, point(x, y));
			return point_test_text_local(p.m_x, p.m_y);
		}

		// SwiffPlayer.dll 0x10088650.
		// [CHOSEN, not recovered] S3 bridge:
		// definition rectangles and optional renderer lines use the native interfaces.
		// The mouse: per-frame state machine on (inside, button) against the previous
		// frame; a press inside starts a drag/selection, its release focuses the field, a press
		// outside a focused field blurs it. [INFERRED: the selection arguments the hit test takes.]
		//  This is the text field's mouse pass (vtable slot 4): it ignores
		// what the siblings above consumed - the argument is unread by the shipped body - so a click
		// reaches the field through any decoration drawn over it, and it answers "inside" (the state's
		// bit 0) for the siblings below. Read-only fields, and every field but the focused one while
		// one is focused, stay out of it. Until 2026-09-13 this port returned the field as stock's
		// "topmost entity", which the Textbox component's bevel shape pre-empted.
		virtual bool	mouse_update(bool /*consumed*/)
		{
			if (m_def->m_readonly)
			{
				return false;
			}
			matrix	world = get_world_matrix();
			get_parent()->get_active_mouse_entity();
			if (s_focused_field != NULL && s_focused_field != this)
			{
				return false;
			}
			int	mx = 0, my = 0, mb = 0;
			get_parent()->get_mouse_state(&mx, &my, &mb);

			m_prev_mouse_state = m_mouse_state;
			m_mouse_state = mb ? 2 : 0;
			point	p;
			world.transform_by_inverse(&p, point((float) mx * 20.0f, (float) my * 20.0f));
			if (point_test_text_local(p.m_x, p.m_y))
			{
				m_mouse_state |= 1;
			}
			// the unscaled local point, for the selection
			world.m_[0][0] = 1.0f;
			world.m_[1][1] = 1.0f;
			world.transform_by_inverse(&p, point((float) mx * 20.0f, (float) my * 20.0f));
			if (m_first_mouse_frame)
			{
				m_prev_mouse_state = m_mouse_state;
				m_first_mouse_frame = false;
			}
			if (m_dragging && m_lines.size() > 0)
			{
				float	x1 = m_drag_x * 0.05f, y1 = m_drag_y * 0.05f;
				float	x2 = p.m_x * 0.05f, y2 = p.m_y * 0.05f;
				int	pos = 0;
				// 0x100887A5..0x100887D7: x2/y2 are fstp'd floats pushed unconverted (no fistp).
				if (render::text_hit(m_lines[0], &pos, x2, y2))
				{
					m_caret = (Uint16) pos;
				}
				m_blink_time = 0;
				if (x1 > x2) { float t = x1; x1 = x2; x2 = t; }
				if (y1 > y2) { float t = y1; y1 = y2; y2 = t; }
				// 0x10088823..0x1008883B: the sorted floats are pushed as raw dwords.
				render::text_set_selection(m_lines[0], x1, y1, x2, y2);
			}
			if (m_mouse_state == m_prev_mouse_state)
			{
				return (m_mouse_state & 1) != 0;
			}
			world.transform_by_inverse(&p, point((float) mx * 20.0f, (float) my * 20.0f));
			switch (m_mouse_state | (m_prev_mouse_state << 2))
			{
			case 12:
			case 13:
				// released after a press inside: focus
				m_dragging = false;
				if (s_focused_field == this) return (m_mouse_state & 1) != 0;
				focus_from_click();
				return (m_mouse_state & 1) != 0;
			case 8:
				if (!m_dragging) return (m_mouse_state & 1) != 0;
				m_dragging = false;
				if (s_focused_field == this) return (m_mouse_state & 1) != 0;
				focus_from_click();
				return (m_mouse_state & 1) != 0;
			case 7:
				// pressed inside
				m_drag_y = p.m_y;
				m_dragging = true;
				m_drag_x = p.m_x;
				return (m_mouse_state & 1) != 0;
			case 2:
				// pressed outside: blur
				if (s_focused_field == this)
				{
					if (m_lines.size() > 0)
					{
						m_caret = 0;
						render::text_caret(m_lines[0], 0);
						render::text_blur(m_lines[0]);
					}
					// 0x100889A0 rechecks focus after the renderer calls.
					if (s_focused_field == this) s_focused_field = NULL;
				}
				return (m_mouse_state & 1) != 0;
			}
			// 0x10088A40 reads the state after focus callbacks have returned.
			return (m_mouse_state & 1) != 0;
		}
		void	focus_from_click()
		{
			m_blink_time = 0;
			s_focused_field = this;
			format_text();
			m_caret = (Uint16) m_text.length();
			if (m_lines.size() > 0)
			{
				render::text_caret(m_lines[0], m_caret);
			}
			call_callback(m_on_set_focus);
		}

		// ---- editing (sub_10088A70 insert, sub_10088DB0 delete, sub_10088D60 delete-selection) ----
		bool	get_selection(int* start, int* end)
		{
			*start = 0; *end = 0;
			if (m_lines.size() == 0) return false;
			//  the RENDERER answers whether a selection exists: the
			// shipped delete_selection (sub_10088D60) tests the Boolean of render::text_selection
			// (sub_1009A910 -> render_handler +112) and only then deletes [start, end). The port's
			// former `start != end` test read the forwarder's scratch start value as a selection
			// whenever the renderer had none, so Backspace and Delete deleted nothing (2026-09-12).
			return render::text_selection(m_lines[0], start, end);
		}
		// SwiffPlayer.dll 0x10088DB0; endpoints are used in the supplied order.
		void	delete_range(int new_length, int end, bool relayout)
		{
			if (new_length == end || new_length < 0) return;
			const int	len = m_text.length();
			if (new_length >= len || end < 0 || end > len) return;
			tu_string	t = m_text;
			t.resize(new_length);
			t += m_text.c_str() + end;
			m_text = t;
			// the wide store is untouched (sub_10088DB0 edits +300 only): a field filled through a
			// String object keeps displaying that text while the narrow copy is edited underneath.
			m_caret = (Uint16) new_length;
			if (relayout) format_text();
			call_callback(m_on_changed);
			if (m_lines.size() > 0) render::text_blur(m_lines[0]);
		}
		// SwiffPlayer.dll 0x10088D60. [CHOSEN, not recovered] S3 bridge:
		// get_selection permits fields with no native renderer line.
		bool	delete_selection(bool relayout)
		{
			int	s = 0, e = 0;
			if (!get_selection(&s, &e)) return false;
			delete_range(s, e, relayout);
			return true;
		}
		// SwiffPlayer.dll 0x10088A70.
		void	insert_text(const char* str)
		{
			delete_selection(false);
			tu_string	ins(str);
			if (m_restrict.length() > 0)
			{
				int kept = 0;
				for (int i = 0; i < ins.length(); i++)
				{
					for (int k = 0; k < m_restrict.length(); k++)
					{
						if (str[i] == m_restrict[k]) { ins[kept++] = str[i]; break; }
					}
				}
				if (kept == 0) return;
				ins[kept] = 0;
				ins.resize(kept);
			}
			tu_string	t = m_text;
			t.resize(m_caret);
			t += ins;
			t += m_text.c_str() + m_caret;
			m_text = t;
			// the wide store is untouched (sub_10088A70 edits +300 only), as in delete_range.
			m_caret = (Uint16) (m_caret + ins.length());
			if (m_max_chars > 0 && m_text.length() > m_max_chars)
			{
				m_text.resize(m_max_chars);
			}
			if (m_caret > m_text.length()) m_caret = (Uint16) m_text.length();
			format_text();
			call_callback(m_on_changed);
			m_blink_time = 0;
		}

		// ---- text_input (the shipped +200 interface) ----
		// SwiffPlayer.dll 0x10088ED0.
		virtual void	on_key(int vkey)
		{
			if (vkey == 9 && m_tab_index > 0)
			{
				if (s_mod_shift ? focus_prev(m_tab_index) : focus_next(m_tab_index)) return;
			}
			if (m_lines.size() == 0) return;
			if (s_mod_ctrl)
			{
				if (vkey == 67) copy();
				else if (vkey == 86) paste();
				else if (vkey == 88) cut();
				return;
			}
			char	ch = 0;
			bool insert_char = true;
			if (vkey >= 65 && vkey <= 90)
			{
				ch = (char) (vkey + (s_mod_shift ? 0 : 0x20));
			}
			else if (vkey == 32)
			{
				ch = ' ';
			}
			else if (vkey >= 48 && vkey <= 57)
			{
				const int	base = 3 * (vkey - 48);
				if (s_mod_shift)
				{
					if (s_mod_alt) return;
					ch = (char) s_digit_keymap[base + 1];
				}
				else if (!s_mod_caps)
				{
					ch = (char) s_digit_keymap[base + (s_mod_alt ? 2 : 0)];
				}
				else
				{
					ch = (char) s_digit_keymap[base + 1];
				}
			}
			else
			{
				switch (vkey)
				{
				case 46:	// Delete
					if (s_mod_alt) return;
					insert_char = false;
					if (!delete_selection(true)) delete_range(m_caret, m_caret + 1, true);
					break;
				case 8:		// Backspace
					if (s_mod_alt) return;
					insert_char = false;
					if (!delete_selection(true)) delete_range(m_caret - 1, m_caret, true);
					break;
				case 37:	// Left
					insert_char = false;
					m_caret = (Uint16) (m_caret > 0 ? m_caret - 1 : 0);
					format_text();
					break;
				case 39:	// Right
					insert_char = false;
					m_caret = (Uint16) (m_caret + 1 < m_text.length() ? m_caret + 1 : m_text.length());
					format_text();
					break;
				case 36:	// Home
					insert_char = false;
					m_caret = 0;
					render::text_caret(m_lines[0], 0);
					render::text_blur(m_lines[0]);
					break;
				case 35:	// End
					insert_char = false;
					m_caret = (Uint16) m_text.length();
					render::text_caret(m_lines[0], m_caret);
					render::text_blur(m_lines[0]);
					break;
				case 189:
				case 190:
					if (s_mod_alt || s_mod_ctrl) return;
					ch = (char) (2 * (vkey == 189) + 43);
					break;
				case 223:
					ch = '.';
					break;
				default:
					return;
				}
			}
			if (insert_char)
			{
				char	s[2] = { ch, 0 };
				insert_text(s);
			}
			m_blink_time = 0;
			if (m_lines.size() > 0) render::text_blur(m_lines[0]);
		}
		// SwiffPlayer.dll 0x10088A50.
		virtual void	on_char(int character)
		{
			char	s[2] = { (char) character, 0 };
			insert_text(s);
		}
		// SwiffPlayer.dll 0x10089280. [CHOSEN, not recovered] S3 bridge:
		// retain the native no-renderer-line case at the blur boundary.
		virtual void	paste()
		{
			tu_string	text;
			if (m_def->m_multiline)
			{
				text = s_text_clipboard;
			}
			else
			{
				int	n = 0;
				while (n < s_text_clipboard.length() && s_text_clipboard[n] != 10 && s_text_clipboard[n] != 13) n++;
				text = s_text_clipboard;
				text.resize(n);
			}
			if (text.length() > 0) insert_text(text.c_str());
			if (m_lines.size() > 0) render::text_blur(m_lines[0]);
		}
		// SwiffPlayer.dll 0x10087BB0; the renderer owns endpoint order and validity.
		virtual void	copy()
		{
			if (m_password) return;
			int	s = 0, e = 0;
			if (!get_selection(&s, &e)) return;
			s_text_clipboard = m_text.c_str() + s;
			s_text_clipboard.resize(e - s);
		}
		// SwiffPlayer.dll 0x100893C0.
		virtual void	cut()
		{
			if (m_password) return;
			copy();
			delete_selection(true);
		}

		// ---- the AS member surface (sub_10089440 set, sub_10087C60 get) ----
		// SwiffPlayer.dll 0x10089440 uses the exact comparison at 0x100858F0.
		// [CHOSEN, not recovered] S3 bridge: the public native key is still tu_stringi.
		bool	dice_set_member(const tu_stringi& member_name, const as_value& val)
		{
			const tu_string& name = member_name.to_tu_string();
			if (name == "_x" || name == "_y")
			{
				matrix m = get_matrix();
				m.m_[name == "_x" ? 0 : 1][2] = bf2x87::store(bf2x87::mul(
					bf2x87::Real(val.to_number()), bf2x87::Real(20.0)));
				set_matrix(m);
				return true;
			}
			if (name == "text")
			{
				// sub_10089440: a DICE String object always goes through the wide setter with its
				// wide text, empty when it carries none (the shipped narrow arm at 0x10089599 is
				// unreachable: it needs a null std::wstring c_str()); any other value goes narrow.
				as_object_interface*	obj = val.to_object_raw();
				if (obj)
				{
					const Uint16*	wide = obj->get_wide_text_value();
					if (wide)
					{
						set_wide_text_value(wide);
					}
					else
					{
						const char*	narrow = obj->get_text_value();
						set_text_value(narrow ? narrow : val.to_string());
					}
				}
				else
				{
					set_text_value(val.to_string());
				}
				return true;
			}
			if (name == "_visible") { set_visible(val.to_bool()); return true; }
			if (name == "_alpha")
			{
				cxform cx = get_cxform();
				const bf2x87::Real alpha = bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(0.01f));
				// Retail selects one for unordered input, then narrows before max.
				const float upper = alpha < bf2x87::Real(1.0f) ? bf2x87::store(alpha) : 1.0f;
				cx.m_[3][0] = tu_util::fmax(0.0f, upper);
				set_cxform(cx);
				return true;
			}
			if (name == "textColor")
			{
				// 0x100896D0 uses a signed dword FISTP, including integer indefinite.
				const Uint32	rgb = (Uint32) bf2x87::toIntTruncate(val.to_number());
				const Uint32	cur = ((Uint32) m_text_color.m_r << 16) | ((Uint32) m_text_color.m_g << 8) | m_text_color.m_b;
				if (rgb != cur)
				{
					m_text_color = rgba((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, 255);
					format_text();
				}
				return true;
			}
			if (name == "_width" || name == "_height")
			{
				const bool	is_w = name == "_width";
				const double	v = val.to_number();
				const bf2x87::Real cur = extent_pixels(is_w);
				if (bf2x87::Real(v) == cur) return true;
				matrix	m = get_matrix();
				float&	s = is_w ? m.m_[0][0] : m.m_[1][1];
				// 0x1008977E..0x100897CE / 0x10089852..0x100898A2:
				// repeat the conversions and extent calls, narrowing the numerator to float.
				if (val.to_number() == 0.0 || extent_pixels(is_w) == bf2x87::Real(0.0f))
				{
					s = 0.0f;
				}
				else
				{
					const float numerator = (float) val.to_number();
					s = bf2x87::store(bf2x87::div(bf2x87::Real(numerator), extent_pixels(is_w)));
				}
				set_matrix(m);
				format_text();
				return true;
			}
			if (name == "border")          { m_border = val.to_bool(); return true; }
			// 0x10089915 / 0x10089999 use the same signed dword conversion.
			if (name == "borderColor")     { const Uint32 c = (Uint32) bf2x87::toIntTruncate(val.to_number()); m_border_color = rgba((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); return true; }
			if (name == "background")      { m_background = val.to_bool(); return true; }
			if (name == "backgroundColor") { const Uint32 c = (Uint32) bf2x87::toIntTruncate(val.to_number()); m_background_color = rgba((c >> 16) & 255, (c >> 8) & 255, c & 255, 255); return true; }
			if (name == "password")        { m_password = val.to_bool(); return true; }
			if (name == "tabIndex")
			{
				// Unregister before conversion: an object conversion can run code.
				if (m_tab_index >= 0 && m_tab_index < s_tab_table.size() && s_tab_table[m_tab_index] == this)
					s_tab_table[m_tab_index] = NULL;
				m_tab_index = bf2x87::lowWord(bf2x87::toInt64Truncate(bf2x87::Real(val.to_number())));
				// As in 0x10089A3B, negative/invalid indices are unchecked here.
				register_text_tab(m_tab_index, this);
				return true;
			}
			if (name == "onChanged")       { m_on_changed = val; return true; }
			if (name == "onSetFocus")      { m_on_set_focus = val; return true; }
			if (name == "maxChars")        { m_max_chars = bf2x87::toIntTruncate(val.to_number()); return true; }
			if (name == "restrict")
			{
				m_restrict = "";
				const char*	s = val.to_string();
				for (const char* p = s; p && *p; ++p)
				{
					if (*p != ' ' && *p != '-') m_restrict += *p;
				}
				return true;
			}
			if (name == "_focus")
			{
				if (val.to_bool())
				{
					m_blink_time = 0;
					s_focused_field = this;
					format_text();
					if (m_lines.size() > 0)
					{
						m_caret = (Uint16) m_text.length();
						render::text_caret(m_lines[0], m_caret);
					}
				}
				else if (s_focused_field == this)
				{
					s_focused_field = NULL;
				}
				return true;
			}
			// Anything else lives in the field's own environment (the shipped falls through to it).
			m_as_environment.set_member(name, val);
			return true;
		}
		// SwiffPlayer.dll 0x10087C60; exact names, then the environment bag.
		// [CHOSEN, not recovered] S3 bridge: native keys remain tu_stringi and
		// native extent virtuals return twips, converted at their consumers.
		bool	dice_get_member(const tu_stringi& member_name, as_value* val)
		{
			const tu_string& name = member_name.to_tu_string();
			if (name == "_x" || name == "_y")
			{
				const matrix m = get_matrix();
				// Both coordinates publish the product directly as double.
				val->set_double(double(bf2x87::mul(
					bf2x87::Real(m.m_[name == "_x" ? 0 : 1][2]), bf2x87::Real(0.05f))));
				return true;
			}
			if (name == "text")
			{
				// The caller may supply a String object to fill in place. Retail
				// extracts OBJECT only and does not validate its concrete type.
				if (as_object_interface* string = val->to_object_raw())
					assign_dice_string_object_narrow(string, m_text.c_str());
				else val->set_tu_string(m_text);
				return true;
			}
			if (name == "_visible") { val->set_bool(get_visible()); return true; }
			if (name == "_alpha")
			{
				val->set_double(double(bf2x87::mul(bf2x87::Real(get_cxform().m_[3][0]), bf2x87::Real(100.0f))));
				return true;
			}
			if (name == "textColor")
			{
				const int color = (int) (((Uint32) m_text_color.m_a << 24) | ((Uint32) m_text_color.m_r << 16) | ((Uint32) m_text_color.m_g << 8) | m_text_color.m_b);
				val->set_int(color);
				return true;
			}
			if (name == "_width")          { val->set_double(double(extent_pixels(true))); return true; }
			if (name == "_height")         { val->set_double(double(extent_pixels(false))); return true; }
			//  sub_10087C60: `textHeight` is vslot 0x10C, the laid-out height formatText
			// last requested (m_text_height_px), not the font size. The loading screen's label
			// component sets `_height = textHeight`; with the font size every multi-line field shrank
			// to one line and the next formatText stopped after the first wrap.
			if (name == "textHeight")      { val->set_int(get_text_height()); return true; }
			if (name == "border")          { val->set_bool(m_border); return true; }
			if (name == "borderColor")     { val->set_int((int) (((Uint32) m_border_color.m_a << 24) | ((Uint32) m_border_color.m_r << 16) | ((Uint32) m_border_color.m_g << 8) | m_border_color.m_b)); return true; }
			if (name == "background")      { val->set_bool(m_background); return true; }
			if (name == "backgroundColor") { val->set_int((int) (((Uint32) m_background_color.m_a << 24) | ((Uint32) m_background_color.m_r << 16) | ((Uint32) m_background_color.m_g << 8) | m_background_color.m_b)); return true; }
			if (name == "password")        { val->set_bool(m_password); return true; }
			if (name == "tabIndex")        { if (m_tab_index < 0) return false; val->set_int(m_tab_index); return true; }
			if (name == "onChanged")       { *val = m_on_changed; return true; }
			if (name == "onSetFocus")      { *val = m_on_set_focus; return true; }
			if (name == "maxChars")        { *val = as_value(m_max_chars); return true; }
			if (name == "restrict")        { *val = as_value(""); return true; }
			if (name == "_focus")          { val->set_bool(s_focused_field == this); return true; }
			if (name == "_parent")
			{
				val->set_as_object_interface(get_parent() ? static_cast<as_object_interface*>(get_parent()) : NULL);
				return true;
			}
			return m_as_environment.get_member(name, val);
		}
	};

	// SwiffPlayer.dll 0x10084D90; the native focus stores the field owner rather
	// than its secondary interface, so compare the adjusted interface pointer.
	text_input::~text_input()
	{
		if (get_focused_text_input() == this) s_focused_field = NULL;
	}

	//  the focused-field surface SwiffPlayer's key/char/clipboard entries use.
	text_input*	get_focused_text_input()
	{
		return s_focused_field;
	}
	void	set_text_input_modifiers(bool shift, bool alt, bool ctrl, bool caps)
	{
		s_mod_shift = shift;
		s_mod_alt = alt;
		s_mod_ctrl = ctrl;
		s_mod_caps = caps;
	}
	tu_string&	text_input_clipboard()
	{
		return s_text_clipboard;
	}


	// SwiffPlayer.dll 0x10089C50.
	// [CHOSEN, not recovered] S3 bridge:
	// native factories return raw storage instead of a smart-pointer result.
	character*	edit_text_character_def::create_character_instance(movie* parent, int id)
	{
		if (m_font == NULL)
		{
			// Resolve the font, if possible.
			m_font = m_root_def->get_font(m_font_id);
			if (m_font == NULL)
			{
				log_error("error: text style with undefined font; font_id = %d\n", m_font_id);
			}
		}

		edit_text_character*	ch = new edit_text_character(parent, this, id);
		ch->set_name(m_default_name.c_str());
		return ch;
	}


	// SwiffPlayer.dll 0x10085CE0. [CHOSEN, not recovered] S3 bridge:
	// the native factory assigns the resource ID when publishing the definition.
	void	define_edit_text_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Read a DefineText tag.
	{
		assert(tag_type == 37);

		Uint16	character_id = in->read_u16();

		edit_text_character_def*	ch = new edit_text_character_def(m);
		IF_VERBOSE_PARSE(log_msg("edit_text_char, id = %d\n", character_id));
		ch->read(in, tag_type, m);

		m->add_character(character_id, ch);
	}

}	// end namespace gameswf


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:

