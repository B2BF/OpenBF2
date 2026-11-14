// [TEMP] [CHOSEN, not recovered] Synthetic text-layout observer.
// Includes the production TU to inspect its records; no game, GPU or content runs.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include "base/tu_file.h"
#include "gameswf/gameswf_profile.h"
#include "gameswf/gameswf_timers.h"
#ifndef BF2_SWF_TEXT_SOURCE
#define BF2_SWF_TEXT_SOURCE "../BF2/External/gameswf/gameswf_text.cpp"
#endif
#include BF2_SWF_TEXT_SOURCE

using namespace gameswf;
static int checks;
static void check(bool condition, const char* message)
{
	++checks;
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
static profile_record* counter(const char* name)
{
	for (int i = 0; i < get_profile_records().size(); ++i)
		if (get_profile_records()[i]->m_name == name) return get_profile_records()[i];
	return NULL;
}
static void word(std::vector<Uint8>& b, int value)
{
	b.push_back(Uint8(value)); b.push_back(Uint8(unsigned(value) >> 8));
}
static void read_font(font& f, movie_definition_sub* dictionary, int leading, int descent)
{
	std::vector<Uint8> payload = { 0x84, 0, 4, 't', 'e', 's', 't' };
	word(payload, 3);
	for (int i = 0; i < 4; ++i) word(payload, 8);
	word(payload, ' '); word(payload, 'A'); word(payload, 'B');
	word(payload, 20); word(payload, descent); word(payload, leading);
	word(payload, 1); word(payload, 2); word(payload, 3);
	payload.insert(payload.end(), 3, 0); // Three zero rectangles.
	word(payload, 2);
	word(payload, 0xffff); word(payload, 'A'); word(payload, 1025);
	word(payload, 'A'); word(payload, 'A'); word(payload, 3);
	std::vector<Uint8> bytes;
	word(bytes, (48 << 6) | int(payload.size()));
	bytes.insert(bytes.end(), payload.begin(), payload.end());
	tu_file input(tu_file::memory_buffer, int(bytes.size()), bytes.data());
	stream s(&input);
	check(s.open_tag() == 48, "synthetic font tag");
	f.read(&s, 48, dictionary);
	check(f.get_glyph_count() == 3 && f.get_glyph_index('A') == 1, "font uses real parser");
}

struct text_observer : gameswf::render_handler
{
	struct line { std::wstring text; float width = 0; int caret = -1; };
	std::vector<std::wstring> publications;
	std::vector<std::string> events;
	float measured_width = 0;
	bool change_margin = false;
	bool verify_tab = false;
	bool verify_construction = false;
	bool null_line = false;
	std::string created_name;
	int created_height = 0;
	edit_text_character_def* definition = NULL;
	bitmap_info* create_bitmap_info_empty() override { std::abort(); }
	void set_depth(int) override {}
	bitmap_info* create_bitmap_info_alpha(int, int, unsigned char*) override { std::abort(); }
	bitmap_info* create_bitmap_info_rgb(image::rgb*) override { std::abort(); }
	bitmap_info* create_bitmap_info_rgba(image::rgba*) override { std::abort(); }
	void delete_bitmap_info(bitmap_info*) override { std::abort(); }
	void begin_display(rgba, int, int, int, int, float, float, float, float) override { std::abort(); }
	void end_display() override { std::abort(); }
	void set_matrix(const matrix&) override { std::abort(); }
	void set_cxform(const cxform&) override { std::abort(); }
	void draw_mesh_strip(const void*, int) override { std::abort(); }
	void draw_line_strip(const void*, int) override { std::abort(); }
	void line_style_disable() override { std::abort(); }
	void line_style_color(rgba) override { std::abort(); }
	void line_style_width(float) override { std::abort(); }
	void fill_style_disable(int) override { std::abort(); }
	void fill_style_color(int, rgba) override { std::abort(); }
	void fill_style_bitmap(int, const bitmap_info*, const matrix&, bitmap_wrap_mode) override { std::abort(); }
	void draw_bitmap(const matrix&, const bitmap_info*, const rect&, const rect&, rgba) override { std::abort(); }
	void set_antialiased(bool) override { std::abort(); }
	void begin_submit_mask() override { std::abort(); }
	void end_submit_mask() override { std::abort(); }
	void disable_mask() override { std::abort(); }
	void* text_create(const char* name, int height) override
	{
		if (verify_construction)
		{
			edit_text_character* pending = s_tab_table[7];
			check(pending && pending->m_dummy_style.size() == 1 && pending->m_text_glyph_records.size() == 0,
				"constructor seeds fill style and Tab entry before renderer callback");
			check(pending->m_as_environment.get_target() == pending && pending->m_lines.size() == 0,
				"constructor publishes environment before requesting first renderer line");
		}
		created_name = name; created_height = height;
		events.push_back("create"); return null_line ? NULL : new line;
	}
	void text_set(void* value, const wchar_t* text) override
	{
		static_cast<line*>(value)->text = text;
		publications.push_back(text); events.push_back("set");
	}
	void text_set_size(void* value, float width, int) override { static_cast<line*>(value)->width = width; }
	bool text_extent(void*, int, float* w, float* h) override
	{
		*w = measured_width; *h = 1;
		if (change_margin) { definition->m_right_margin = 20; change_margin = false; }
		return true;
	}
	void text_caret(void* value, int caret) override { static_cast<line*>(value)->caret = caret; events.push_back("caret"); }
	void text_release(void* value) override
	{
		if (verify_tab) check(s_tab_table[7] == NULL, "Tab removal precedes renderer release callback");
		events.push_back("release"); delete static_cast<line*>(value);
	}
};

static void push_string(array<Uint8>& code, const char* text)
{
	const int length = int(std::strlen(text)) + 2;
	code.push_back(0x96); code.push_back(Uint8(length)); code.push_back(Uint8(length >> 8));
	code.push_back(0);
	for (const char* p = text;; ++p) { code.push_back(Uint8(*p)); if (!*p) break; }
}

static void timer_after_movie_release_checks()
{
	// Synthetic empty movie with an empty sprite definition at ID 1. A second,
	// live movie owns the interval; its saved argument keeps the old field alive.
	// Modes cover live children, pending removal, and equal-depth displacement.
	for (int mode = 0; mode != 3; ++mode)
	{
		unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x1c, 0, 0, 0,
			0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0,
			0xc6, 0x09, 1, 0, 1, 0, 0, 0, // DefineSprite 39, ID 1, one empty frame.
			0x40, 0, 0, 0 };
		tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
		smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
			DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
		movie_definition_sub* dictionary = static_cast<movie_definition_sub*>(definition.get_ptr());
		smart_ptr<font> exported_font = new font;
		dictionary->add_font(7, exported_font.get_ptr());
		dictionary->export_resource(7, "SyntheticFont", exported_font.get_ptr());
		smart_ptr<edit_text_character_def> text_def = new edit_text_character_def(dictionary);
		text_def->m_font_id = 7;
		text_def->m_rect = rect{0, 2000, 0, 2000};
		dictionary->add_character(2, text_def.get_ptr());
		text_observer handler;
		set_render_handler(&handler);
		// create_instance returns an already-retained interface, as releaseMovie expects.
		movie_interface* released_movie = definition->create_instance();
		movie* released_root = released_movie->get_root_movie();
		const array<swf_event*> events;
		character* page = released_root->add_display_object(1, "page", events, 1, false,
			cxform::identity, matrix::identity, 0, 0);
		smart_ptr<character> field = page->add_display_object(2, "label", events, 2, false,
			cxform::identity, matrix::identity, 0, 0);
		check(field != NULL && field->get_parent() == page, "timer fixture has a nested text field");
		field->set_member("text", as_value("before"));
		check(!handler.publications.empty() && handler.publications.back() == L"before",
			"attached text setter formats renderer lines immediately");

		movie_interface* live_movie = definition->create_instance();
		movie* receiver = live_movie->get_root_movie();
		as_environment* env = receiver->get_action_environment();
		array<Uint8> code;
		push_string(code, "field"); code.push_back(0x1c); // GetVariable
		push_string(code, "text"); push_string(code, "after");
		code.push_back(0x4f); code.push_back(0); // SetMember; End
		tu_file actions(tu_file::memory_buffer, code.size(), &code[0]);
		stream action_stream(&actions);
		action_buffer buffer;
		buffer.read(&action_stream);
		const array<with_stack_entry> scope;
		smart_ptr<as_as_function> callback = new as_as_function(&buffer, env, 0, scope);
		callback->set_length(buffer.get_length()); callback->add_arg("field");
		receiver->set_member("tick", as_value(callback.get_ptr()));
		env->push(field.get_ptr()); env->push(1); env->push("tick"); env->push(receiver);
		as_value id;
		timer_setinterval(fn_call(&id, receiver, env, 4, env->get_top_index()));
		env->drop(4);

		if (mode == 1)
		{
			page->remove_display_object(2);
			check(field->get_parent() == NULL, "pending removal detaches before the sweep");
		}
		if (mode == 2)
		{
			page->add_display_object(1, "replacement", events, 2, false,
				cxform::identity, matrix::identity, 0, 0);
			check(field->get_parent() == NULL, "equal-depth displacement detaches the old field");
		}
		// This actually frees the old root and page; the global root is the live
		// interval owner's movie. No global teardown cancels the saved callback.
		released_movie->drop_ref();
		const size_t before = handler.publications.size();
		update_intervals(1.0f);
		check(handler.publications.size() > before && handler.publications.back() == L"after",
			"timer executes AVM1 text assignment after the containing movie is freed");
		check(field->get_parent() == NULL, "surviving field has no dangling or replacement parent");
		const matrix world = field->get_world_matrix();
		check(world.m_[0][0] == 1 && world.m_[1][1] == 1,
			"detached text field uses its local world matrix");
		check(env->get_top_index() == -1, "timer callback balances the live environment stack");
		env->push_val(id);
		timer_clearinterval(fn_call(&id, receiver, env, 1, env->get_top_index()));
		env->drop(1);
		receiver->set_member("tick", as_value());
		callback = NULL;
		field = NULL;
		live_movie->drop_ref();
		set_render_handler(NULL);
	}
}

int main()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x1f, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0,
		0x49, 0x09, 1, 0, 0x30, 0x0a, 0, 0xa0, 0, 0, 0,
		0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> movie_def = create_movie_no_recurse(&input, DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> movie_instance = movie_def->create_instance();
	movie* root = movie_instance->get_root_movie();
	movie_definition_sub* dictionary = static_cast<movie_definition_sub*>(movie_def.get_ptr());
	smart_ptr<edit_text_character_def> def = new edit_text_character_def(dictionary);
	check(def->m_root_def == dictionary && def->m_font == NULL && def->m_font_id == -1,
		"text definition retains root and empty font defaults");
	check(!def->m_word_wrap && !def->m_multiline && !def->m_password && !def->m_readonly
		&& !def->m_auto_size && !def->m_no_select && !def->m_border && !def->m_html && !def->m_use_outlines,
		"text definition clears all nine flags");
	check(def->m_text_height == 1 && def->m_max_length == 1024 && def->m_alignment == edit_text_character_def::ALIGN_LEFT,
		"text definition height, maximum and alignment defaults");
	check(def->m_left_margin == 0 && def->m_right_margin == 0 && def->m_indent == 0 && def->m_leading == 0,
		"text definition clears four layout floats");
	check(def->m_color.m_r == 0 && def->m_color.m_g == 0 && def->m_color.m_b == 0 && def->m_color.m_a == 255
		&& def->m_default_name.length() == 0 && def->m_default_text.length() == 0,
		"text definition black color and empty strings");
	def->m_rect = rect{0, 100000, 0, 100000};
	smart_ptr<edit_text_character> field = new edit_text_character(root, def.get_ptr(), 1);
	const int records = get_profile_records().size();
	field->format_text();
	check(get_profile_records().size() == records, "no-font exit precedes profiler registration");
	smart_ptr<font> f = new font;
	read_font(*f.get_ptr(), dictionary, -1024, 1);
	def->m_font = f.get_ptr();
	def->m_text_height = std::nextafter(1024.0f, 2048.0f);
	set_profiling(false);
	field->format_text();
	profile_record* glyph = counter("formatText");
	check(glyph && glyph->m_count == 1, "glyph formatting counts with profiling disabled");
	const text_glyph_record& empty = field->m_text_glyph_records[0];
	check(empty.m_style.m_y_offset == std::nextafter(-1.0f, -2.0f), "baseline retains product through cancellation");
	check(field->m_caret_y == 40, "initial glyph caret adds descent and padding before truncation");
	set_profiling(true);
	field->format_text();
	check(glyph->m_count == 2 && glyph->m_start != 0, "enabled glyph formatter starts and counts");
	const uint64 start = glyph->m_start;
	set_profiling(false);
	field->format_text();
	check(glyph->m_count == 3 && glyph->m_start == start, "disabled formatter retains previous start");
	def->m_rect.m_x_min = -1025.0001220703125f;
	field->m_text = "A";
	field->format_text();
	check(field->m_text_glyph_records[0].m_style.m_x_offset == def->m_rect.m_x_min, "record keeps pre-kerning origin");
	check(field->m_caret_x == -1025, "caret tracks original record start");
	def->m_rect.m_x_min = 0;
	def->m_text_height = 1024;
	field->m_text_color = rgba(1,2,3,4);
	def->m_color = rgba(5,6,7,8);
	field->m_text = "A\r\nB";
	field->format_text();
	check(field->m_text_glyph_records.size() == 3, "CR and LF start distinct records");
	check(field->m_text_glyph_records[0].m_style.m_color.m_r == 1
		&& field->m_text_glyph_records[1].m_style.m_color.m_r == 5, "new paragraph restores definition color");
	check(field->m_text_glyph_records[0].m_glyphs.size() == 1
		&& field->m_text_glyph_records[1].m_glyphs.size() == 0
		&& field->m_text_glyph_records[2].m_glyphs.size() == 1, "newline glyph distribution");
	field->m_text = "?";
	field->format_text();
	check(field->m_text_glyph_records.size() == 1 && field->m_text_glyph_records[0].m_glyphs.size() == 0,
		"missing glyph is omitted");
	def->m_word_wrap = true;
	def->m_rect.m_x_max = 2.5f;
	field->m_text = "B";
	field->m_caret = 0;
	field->format_text();
	check(field->m_text_glyph_records.size() == 2 && field->m_text_glyph_records[0].m_glyphs.size() == 0
		&& field->m_text_glyph_records[1].m_glyphs.size() == 1, "unbroken overflow moves last glyph to next record");
	check(field->m_caret_x == 0, "overflow caret is set before moving glyph");
	def->m_rect.m_x_max = 5.5f;
	field->m_text = "B B";
	field->m_caret = 3;
	field->format_text();
	check(field->m_text_glyph_records.size() == 2 && field->m_text_glyph_records[0].m_glyphs.size() == 1
		&& field->m_text_glyph_records[1].m_glyphs.size() == 1, "word wrap removes space and transfers tail");
	check(field->m_caret_x == 3, "word-tail caret follows moved advance");
	def->m_word_wrap = false;
	field->m_text = "BBB\nB";
	field->format_text();
	check(field->m_text_glyph_records.size() == 2 && field->m_text_glyph_records[0].m_glyphs.size() == 1
		&& field->m_text_glyph_records[1].m_glyphs.size() == 1, "nonwrapping overflow scans through newline");
	def->m_rect.m_y_max = -100000;
	field->m_text = "B";
	field->format_text();
	check(field->m_text_glyph_records.size() == 1 && field->m_text_glyph_records[0].m_glyphs.size() == 0,
		"height clipping discards unfinished glyph record");
	def->m_rect.m_y_max = 100000;

	text_observer handler;
	set_render_handler(&handler);
	const Uint32 glyph_count_before_constructors = glyph->m_count;
	root->set_member("tabIndex", as_value(7));
	dictionary->add_font(7, f.get_ptr());
	dictionary->export_resource(7, "ExportedFont", f.get_ptr());
	dictionary->export_resource(8, "", f.get_ptr());
	smart_ptr<edit_text_character_def> ctor_def = new edit_text_character_def(dictionary);
	ctor_def->m_font_id = 7;
	ctor_def->m_text_height = 60;
	handler.verify_construction = true;
	smart_ptr<edit_text_character> constructed = new edit_text_character(root, ctor_def.get_ptr(), 9);
	check(handler.created_name == "ExportedFont" && handler.created_height == 3,
		"constructor resolves declared font ID without requiring a font pointer");
	check(constructed->m_lines.size() == 1 && constructed->m_lines[0] != NULL,
		"constructor owns first renderer result for empty text");
	as_value removed_width;
	check(!constructed->get_member("textWidth", &removed_width),
		"retail text fields do not publish the 2006 textWidth property");
	handler.verify_construction = false;
	constructed = NULL;
	ctor_def->m_font_id = 8;
	ctor_def->m_text_height = std::numeric_limits<float>::quiet_NaN();
	handler.null_line = true;
	constructed = new edit_text_character(root, ctor_def.get_ptr(), 10);
	check(handler.created_name.empty() && handler.created_height == (std::numeric_limits<int>::min)(),
		"constructor forwards empty export and integer-indefinite font height");
	check(constructed->m_lines.size() == 1 && constructed->m_lines[0] == NULL,
		"constructor appends null renderer result");
	constructed = NULL;
	handler.null_line = false;
	ctor_def->m_font_id = -1;
	ctor_def->m_font = f.get_ptr();
	ctor_def->m_default_text = "B";
	ctor_def->m_text_height = 1024;
	ctor_def->m_rect = rect{0, 100000, 0, 100000};
	constructed = new edit_text_character(root, ctor_def.get_ptr(), 11);
	check(constructed->m_text == "B" && constructed->m_text_glyph_records.size() > 0,
		"constructor formats its default text into glyph records");
	constructed = NULL;
	handler.events.clear(); handler.publications.clear();
	const Uint32 constructor_formats = glyph->m_count - glyph_count_before_constructors;
	field->m_font_name = "synthetic";
	field->m_lines.push_back(handler.text_create("synthetic", 1));
	field->m_wide_text.push_back('A'); field->m_wide_text.push_back(0);
	field->m_text = "B";
	def->m_text_height = std::nextafter(20.0f, 40.0f);
	def->m_leading = -1;
	def->m_word_wrap = false;
	field->format_text();
	profile_record* objects = counter("formatTextObjects");
	check(objects && objects->m_count == 1, "renderer-line formatting counts with profiling disabled");
	check(handler.publications.front() == L"A", "wide source wins over narrow text");
	check(field->m_text_height_px == -50, "height conversion keeps full expression before dword truncation");
	check(glyph->m_count == 10 + constructor_formats, "renderer-line branch bypasses glyph profiler");
	set_profiling(true);
	field->format_text();
	check(objects->m_count == 2 && objects->m_start != 0, "renderer-line enabled start and count");
	set_profiling(false);
	def->m_word_wrap = true;
	def->m_right_margin = 0;
	field->m_wide_text[0] = 'B';
	handler.definition = def.get_ptr();
	handler.change_margin = true;
	handler.publications.clear();
	field->format_text();
	check(field->m_lines.size() == 2 && handler.publications[1] == L"",
		"wrap margin is read after the renderer extent callback");
	check(objects->m_count == 3, "reentrant renderer callback keeps one format count");
	field->set_tab_index(7);
	handler.verify_tab = true;
	field = NULL;
	set_render_handler(NULL);
	release_script_objects();
	timer_after_movie_release_checks();
	std::printf("swf_text_layout_runtime: %d checks passed\n", checks);
}
