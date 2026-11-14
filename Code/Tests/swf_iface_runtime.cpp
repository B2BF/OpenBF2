// [TEMP] [CHOSEN, not recovered] Synthetic observers for S3 interface recovery.
// No engine startup, renderer device, game content or network is used.
#include <cmath>
#include <cstdio>
#include <type_traits>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <initializer_list>
#include <limits>
#include <vector>
#include "gameswf/gameswf_action.h"
#include "gameswf/gameswf_impl.h"
#include "gameswf/gameswf_render.h"
#include "gameswf/gameswf_profile.h"
#include "gameswf/gameswf_shape.h"
#include "gameswf/gameswf_stream.h"
#include "gameswf/gameswf_timers.h"
#include "gameswf/gameswf_button.h"
#include "gameswf/gameswf_font.h"
#include "base/tu_file.h"
#include "base/image.h"

using namespace gameswf;
static int checks;
static void check(bool condition, const char* description)
{
	++checks;
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", description); std::abort(); }
}

static char logged_bytes[500];
static int log_calls;
static bool log_was_error;
static void observe_log(bool error, const char* message)
{
	std::memcpy(logged_bytes, message, sizeof(logged_bytes));
	log_was_error = error; ++log_calls;
}
static void log_buffer_checks()
{
	register_log_callback(NULL);
	int written = 71;
	log_msg("%n", &written);
	log_error("%n", &written);
	check(written == 71, "missing log callback skips formatting side effects");
	char input[1501];
	std::memset(input, 'x', sizeof(input)); input[1500] = 0;
	register_log_callback(observe_log);
	for (int length : { 498, 499, 500, 501, 1500 })
	{
		const char saved = input[length]; input[length] = 0;
		const int before = log_calls;
		log_msg("%s", input);
		check(log_calls == before + 1 && !log_was_error, "informational log callback and flag");
		const int bytes = length < 500 ? length : 500;
		bool matched = true;
		for (int i = 0; i < bytes; ++i) matched = matched && logged_bytes[i] == 'x';
		check(matched, "log forwards all 500 bytes without reserving a terminator");
		if (length < 500) check(logged_bytes[length] == 0, "short log terminates");
		input[length] = saved;
	}
	log_error("short:%d", 7);
	check(log_was_error && std::strcmp(logged_bytes, "short:7") == 0, "error log callback and flag");
	check(logged_bytes[8] == 'x' && logged_bytes[499] == 'x', "short log preserves trailing shared workspace bytes");
	register_log_callback(NULL);
}

struct observer : gameswf::render_handler
{
	int calls = 0;
	const void* coordinates = NULL;
	int vertices = 0;
	int images = 0;
	int depth = 0;
	int bitmap_releases = 0;
	unsigned released_texture = 0;
	bool named_type_at_release = false;
	int image_width = 0;
	int image_height = 0;
	Uint8 first_pixel[4] = {};
	Uint8 last_pixel[4] = {};
	bitmap_info* create_bitmap_info_empty() override { std::abort(); }
	bitmap_info* create_bitmap_info_alpha(int, int, unsigned char*) override { std::abort(); }
	bitmap_info* create_bitmap_info_rgb(image::rgb*) override { std::abort(); }
	bitmap_info* create_bitmap_info_rgba(image::rgba* im) override
	{
		++images;
		image_width = im->m_width;
		image_height = im->m_height;
		std::memcpy(first_pixel, im->m_data, 4);
		std::memcpy(last_pixel, im->m_data + (im->m_width - 1) * 4, 4);
		return new bitmap_info;
	}
	void delete_bitmap_info(bitmap_info* bi) override
	{
		++bitmap_releases;
		released_texture = bi->m_texture_id;
		named_type_at_release = dynamic_cast<named_bitmap_info*>(bi) != NULL;
	}
	void set_depth(int value) override { depth = value; }
	void begin_display(rgba c, int x, int y, int w, int h, float x0, float x1, float y0, float y1) override
	{
		check(c.m_r == 7 && x == 1 && y == 2 && w == 3 && h == 4
			&& x0 == 5 && x1 == 6 && y0 == 7 && y1 == 8, "begin display arguments"); ++calls;
	}
	void end_display() override { ++calls; }
	void set_matrix(const matrix&) override { std::abort(); }
	void set_cxform(const cxform&) override { std::abort(); }
	void draw_mesh_strip(const void* p, int n) override { coordinates = p; vertices = n; ++calls; }
	void draw_line_strip(const void* p, int n) override { coordinates = p; vertices = n; ++calls; }
	void fill_style_disable(int) override { std::abort(); }
	void fill_style_color(int, rgba) override { std::abort(); }
	void fill_style_bitmap(int, const bitmap_info*, const matrix&, bitmap_wrap_mode) override { std::abort(); }
	void line_style_disable() override { std::abort(); }
	void line_style_color(rgba) override { std::abort(); }
	void line_style_width(float) override { std::abort(); }
	void draw_bitmap(const matrix&, const bitmap_info* b, const rect& r, const rect& uv, rgba c) override
	{
		check(b == NULL && r.m_x_min == 2 && uv.m_x_min == 2 && c.m_r == 7, "bitmap arguments"); ++calls;
	}
	void text_set(void* line, const wchar_t* text) override
	{
		check(line == this && std::wcscmp(text, L"sample") == 0, "text arguments"); ++calls;
	}
	void text_draw(void* line, const matrix* m, unsigned color, int flags) override
	{
		check(line == this && m != NULL && color == 9 && flags == 11, "text draw arguments"); ++calls;
	}
	void set_antialiased(bool) override { std::abort(); }
	void begin_submit_mask() override { std::abort(); }
	void end_submit_mask() override { std::abort(); }
	void disable_mask() override { std::abort(); }
};

static void draw_group(observer& handler)
{
	matrix m;
	const rect r = { 2, 4, 6, 8 };
	const Sint16 coordinates[] = { 1, 2, 3, 4 };
	render::begin_display(rgba(7, 8, 9, 10), 1, 2, 3, 4, 5, 6, 7, 8);
	render::draw_mesh_strip(coordinates, 2);
	check(handler.coordinates == coordinates && handler.vertices == 2, "mesh arguments");
	render::draw_line_strip(coordinates, 1);
	check(handler.coordinates == coordinates && handler.vertices == 1, "line arguments");
	render::draw_bitmap(m, NULL, r, r, rgba(7, 8, 9, 10));
	render::text_set(&handler, L"sample");
	render::text_draw(&handler, &m, 9, 11);
	render::end_display();
}

static profile_record* record(const char* name)
{
	const array<profile_record*>& records = get_profile_records();
	for (int i = 0; i < records.size(); ++i)
		if (std::strcmp(records[i]->m_name.c_str(), name) == 0) return records[i];
	return NULL;
}

static unsigned short async_key_state;
static int async_queries;
static unsigned short query_async_key(int code)
{
	check(code == 13, "asynchronous source is queried only for Enter");
	++async_queries;
	const unsigned short state = async_key_state;
	async_key_state &= 0x8000;
	return state;
}

static void native_key_checks()
{
	set_async_key_state_callback(query_async_key);
	async_queries = 0; async_key_state = 0x8001;
	for (int code : { -1, 0, 12, 14, 65, 223, 224, 1000000 })
		check(!is_key_down(code), "non-Enter key refuses without consuming state");
	check(async_queries == 0, "range and Enter guards precede OS query");
	check(is_key_down(13) && async_queries == 1, "Enter high and low bits are both required");
	check(!is_key_down(13), "held Enter does not repeat its consumed press");
	async_key_state = 1;
	check(!is_key_down(13) && async_key_state == 0, "released press is consumed and false");
	async_key_state = 0x8003;
	check(!is_key_down(13), "predicate compares the entire short to 0x8001");

	as_environment env;
	const array<with_stack_entry> scope;
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	env.m_target = instance->get_root_movie();
	(void)get_global_object();
	as_value key_object = env.get_variable("Key", scope), method;
	check(key_object.to_object() && key_object.to_object()->get_member("isDown", &method),
		"registered Key.isDown in the module table");
	check(!get_global_object()->get_member("Key", &method), "no 2006 _global.Key alias");
	key_object.to_object()->get_member("isDown", &method);
	as_c_function_ptr function = method.to_c_function();
	check(function != NULL, "Key.isDown native callback");
	as_value result(71);
	const int before = async_queries;
	function(fn_call(&result, NULL, &env, 0, env.get_top_index()));
	check(result.to_number() == 71 && async_queries == before, "arity refusal leaves result and state untouched");
	for (double input : { 13.9, 12.9, -0.5, 224.0, std::numeric_limits<double>::quiet_NaN(),
		std::numeric_limits<double>::infinity(), 2147483648.0 })
	{
		async_key_state = 0x8001;
		env.push(input);
		function(fn_call(&result, NULL, &env, 1, env.get_top_index()));
		env.drop(1);
		check(result.get_type() == as_value::BOOLEAN && result.to_bool() == (input == 13.9),
			"Key.isDown truncates and publishes the Enter predicate without using its receiver");
	}
	set_async_key_state_callback(NULL);
}

static void bitmap_ownership_checks(observer& handler)
{
	set_render_handler(&handler);
	// [TEMP] A cached resource must never release its handle in a different
	// currently installed handler. Its own resource destructor owns the release.
	struct owned_resource : bitmap_info::render_resource
	{
		int& destroyed;
		explicit owned_resource(int& count) : destroyed(count) {}
		~owned_resource() override { ++destroyed; }
	};
	int destroyed = 0;
	const int before_owned = handler.bitmap_releases;
	{
		bitmap_info owned;
		owned.m_texture_id = 123;
		owned.m_render_resource.reset(new owned_resource(destroyed));
	}
	check(destroyed == 1 && handler.bitmap_releases == before_owned,
		"cached resource tears down without notifying an unrelated current handler");
	const int before = handler.bitmap_releases;
	{
		smart_ptr<named_bitmap_info> bi = new named_bitmap_info;
		check(bi->m_name.empty() && bi->m_texture_id == 0 && bi->m_original_width == 0
			&& bi->m_original_height == 0, "named descriptor initializes name and bitmap fields");
		bi->m_name.assign(1000, 'x'); bi->m_texture_id = 123;
		check(handler.bitmap_releases == before, "retained bitmap does not notify destruction");
	}
	check(handler.bitmap_releases == before + 1 && handler.released_texture == 123
		&& !handler.named_type_at_release, "one notification after named descriptor teardown");
	set_render_handler(NULL);
	bitmap_info* no_handler = render::create_bitmap_info_empty();
	set_render_handler(&handler);
	delete no_handler;
	check(handler.bitmap_releases == before + 2, "fallback bitmap uses current handler and notifies once");
	set_render_handler(NULL);
	delete new named_bitmap_info;
	check(handler.bitmap_releases == before + 2, "absent handler skips notification");
}

static void bitmap_export_checks(observer& handler)
{
	// [TEMP] Synthetic SWF with one bitmap and two aliases; no game data.
	std::vector<Uint8> bytes = { 0x46, 0x57, 0x53, 0x07, 0, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0 };
	const auto word = [&](int value) { bytes.push_back(Uint8(value)); bytes.push_back(Uint8(value >> 8)); };
	word((6 << 6) | 2); word(1); // DefineBits: descriptor-only retail JPEG path.
	const char first[] = "first-export";
	const char last[] = "last-export";
	word((56 << 6) | int(2 + 2 + sizeof(first) + 2 + sizeof(last)));
	word(2); word(1); bytes.insert(bytes.end(), first, first + sizeof(first));
	word(1); bytes.insert(bytes.end(), last, last + sizeof(last));
	word(1 << 6); word(0);
	for (int i = 0; i < 4; ++i) bytes[4 + i] = Uint8(bytes.size() >> (8 * i));
	tu_file input(tu_file::memory_buffer, int(bytes.size()), bytes.data());
	set_render_handler(&handler);
	{
		smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
			DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
		check(definition != NULL && definition->get_bitmap_info_count() == 1, "bitmap tag constructs named descriptor");
		bitmap_info* bi = definition->get_bitmap_info(0);
		check(bi->is_named(), "named bitmap predicate distinguishes external images");
		check(static_cast<named_bitmap_info*>(bi)->m_name == last, "last export alias writes the shared descriptor name");
		check(bi->m_texture_id == unsigned(-1) && bi->m_original_width == 256
			&& bi->m_original_height == 256, "descriptor keeps shipped unloaded handle and dimensions");
	}
	bitmap_info generated;
	check(!generated.is_named(), "generated bitmap is excluded from named texture resolution");
	set_render_handler(NULL);
}

static void gradient_checks(observer& handler)
{
	// A synthetic one-pixel, one-frame FWS movie: no assets or script content.
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	check(definition != NULL, "synthetic movie definition");
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	as_environment* env = root->get_action_environment();
	as_value method;
	check(root->get_member("beginGradientFill", &method), "gradient registration");
	as_c_function_ptr function = method.to_c_function();
	check(function != NULL, "gradient native callback");
	smart_ptr<as_object> colors = create_array_object();
	smart_ptr<as_object> alphas = create_array_object();
	smart_ptr<as_object> ratios = create_array_object();
	array_object_push(colors.get_ptr(), as_value(0xff0000));
	array_object_push(colors.get_ptr(), as_value(0x0000ff));
	array_object_push(alphas.get_ptr(), as_value(50));
	array_object_push(alphas.get_ptr(), as_value(100));
	array_object_push(ratios.get_ptr(), as_value(0));
	array_object_push(ratios.get_ptr(), as_value(255));
	smart_ptr<as_object> box = new as_object;
	box->set_member("matrixType", as_value("box"));
	box->set_member("x", as_value(10)); box->set_member("y", as_value(20));
	box->set_member("w", as_value(100)); box->set_member("h", as_value(50));
	box->set_member("r", as_value(0));
	as_value result(77);
	function(fn_call(&result, root, env, 0, env->get_top_index()));
	check(result.to_number() == 77 && handler.images == 0, "gradient arity refusal preserves result");
	const auto invoke = [&](const char* type)
	{
		env->push(box.get_ptr()); env->push(ratios.get_ptr()); env->push(alphas.get_ptr());
		env->push(colors.get_ptr()); env->push(type);
		function(fn_call(&result, root, env, 5, env->get_top_index()));
		env->drop(5);
	};
	box->set_member("matrixType", as_value("other"));
	invoke("linear");
	check(handler.images == 0 && result.to_number() == 77, "unsupported matrix preserves state");
	box->set_member("matrixType", as_value("box"));
	invoke("linear");
	check(handler.images == 1 && handler.image_width == 256 && handler.image_height == 1, "linear gradient bitmap");
	check(handler.first_pixel[0] == 255 && handler.first_pixel[1] == 0 && handler.first_pixel[2] == 0 && handler.first_pixel[3] == 127, "first gradient record and alpha truncation");
	check(handler.last_pixel[0] == 0 && handler.last_pixel[1] == 0 && handler.last_pixel[2] == 255 && handler.last_pixel[3] == 255, "last gradient record");
	check(result.to_number() == 77, "gradient result untouched");
	as_value canvas;
	check(root->get_member("_____defaultShape", &canvas), "gradient canvas created");
	generic_character* character = static_cast<generic_character*>(canvas.to_object()->cast_to_character());
	shape_character_def* shape = static_cast<shape_character_def*>(character->m_def.get_ptr());
	check(shape->get_fill_styles().size() == 1 && shape->get_fill_styles()[0].get_type() == 0x10, "gradient fill style");
	check(shape->get_paths().size() == 2 && shape->get_paths()[0].m_new_shape
		&& !shape->get_paths()[1].m_new_shape && shape->get_paths()[1].m_fill0 == 1, "marker and one-based fill path");
	invoke("radial");
	check(handler.images == 2 && handler.image_width == 64 && handler.image_height == 64, "radial gradient bitmap");
	check(shape->get_fill_styles().size() == 2 && shape->get_paths().size() == 4
		&& shape->get_paths()[3].m_fill0 == 2, "radial appends without endFill path");
	release_script_objects();
}

static int native_timer_calls;
static int clearing_timer_id;
static void native_timer(const fn_call&) { ++native_timer_calls; }
static void clear_current_timer(const fn_call& fn)
{
	fn.env->push(clearing_timer_id);
	as_value unchanged;
	timer_clearinterval(fn_call(&unchanged, fn.this_ptr, fn.env, 1, fn.env->get_top_index()));
	fn.env->drop(1);
}

static void push_string(array<Uint8>& code, const char* text)
{
	const int length = int(std::strlen(text)) + 2;
	code.push_back(0x96); code.push_back(Uint8(length)); code.push_back(Uint8(length >> 8));
	code.push_back(0);
	for (const char* p = text;; ++p) { code.push_back(Uint8(*p)); if (!*p) break; }
}

static int s4_commands;
static void s4_fscommand(movie_interface*, const char* command, const char* argument)
{
	++s4_commands;
	check(std::strcmp(command, "s4") == 0 && std::strcmp(argument, "argument") == 0,
		"FSCommand keeps its command and argument");
}

static void s4_removal_checks()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	as_value value;
	for (const char* name : { "prevFrame", "getBytesLoaded", "getBytesTotal", "getURL", "swapDepths" })
		check(!root->get_member(name, &value), "retail-absent MovieClip method is unpublished");
	for (const char* name : { "TextFormat", "MovieClipLoader", "XMLSocket", "Array", "Number",
		"Boolean", "String", "Object", "Color", "trace", "math", "Key" })
		check(!get_global_object()->get_member(name, &value), "2006 global publication is removed");
	as_environment* env = root->get_action_environment();
	const array<with_stack_entry> scope;
	as_value math = env->get_variable("math", scope);
	for (const char* name : { "e", "ln2", "log2e", "ln10", "log10e", "pi", "sqrt1_2", "sqrt2" })
		check(math.to_object() && math.to_object()->get_member(name, &value),
			"retail-attested lowercase Math constant remains");
	register_log_callback(observe_log);
	for (Uint8 opcode : { Uint8(0x44), Uint8(0x46), Uint8(0x2a), Uint8(0x2b), Uint8(0x2c) })
	{
		unsigned char code[] = { opcode, 0 };
		tu_file actions(tu_file::memory_buffer, sizeof(code), code);
		stream instructions(&actions); action_buffer buffer; buffer.read(&instructions);
		env->push("sentinel"); const int before = log_calls;
		buffer.execute(env);
		check(env->get_top_index() == 0 && std::strcmp(env->top(0).to_string(), "sentinel") == 0,
			"removed opcode implementation preserves operands");
		check(log_calls - before == (opcode == 0x44 || opcode == 0x46 ? 1 : 0),
			"TypeOf/Enumerate log once and absent opcodes use the quiet default");
		env->pop();
	}
	register_fscommand_callback(s4_fscommand);
	for (const char* url : { "FSCommand:s4", "absent-external-movie.swf" })
	{
		array<Uint8> code;
		const int length = int(std::strlen(url) + std::strlen("argument") + 2);
		code.push_back(0x83); code.push_back(Uint8(length)); code.push_back(Uint8(length >> 8));
		for (const char* text : { url, "argument" })
			for (const char* p = text;; ++p) { code.push_back(Uint8(*p)); if (!*p) break; }
		code.push_back(0);
		tu_file actions(tu_file::memory_buffer, code.size(), &code[0]);
		stream instructions(&actions); action_buffer buffer; buffer.read(&instructions);
		const int before = log_calls; buffer.execute(env);
		check(log_calls == before, "GetURL has no external movie-loader diagnostics");
	}
	for (const char* url : { "FSCommand:s4", "absent-external-movie.swf" })
	{
		array<Uint8> code;
		push_string(code, "sentinel"); push_string(code, url); push_string(code, "argument");
		for (Uint8 byte : { Uint8(0x9a), Uint8(1), Uint8(0), Uint8(0), Uint8(0x3e), Uint8(0) })
			code.push_back(byte);
		tu_file actions(tu_file::memory_buffer, code.size(), &code[0]);
		stream instructions(&actions); action_buffer buffer; buffer.read(&instructions);
		as_value result;
		const int before = log_calls;
		buffer.execute(env, 0, buffer.get_length(), &result, scope, false);
		check(log_calls == before && env->get_top_index() == -1
			&& std::strcmp(result.to_string(), "sentinel") == 0,
			"GetURL2 preserves its stack drop without external movie loading");
	}
	check(s4_commands == 2, "both URL opcodes dispatch FSCommand only");
	{
		unsigned char code[] = { 0x53, 0 };
		tu_file actions(tu_file::memory_buffer, sizeof(code), code);
		stream instructions(&actions); action_buffer buffer; buffer.read(&instructions);
		env->push(0); env->push(as_value(native_timer)); env->push(as_value());
		const int before = native_timer_calls; buffer.execute(env);
		check(env->get_top_index() == 2 && native_timer_calls == before,
			"NewMethod has no direct native-constructor form");
		env->drop(3);
	}
	register_fscommand_callback(NULL);
	register_log_callback(NULL);
	release_script_objects();
}

static void timer_checks()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	as_environment* env = root->get_action_environment();
	root->set_member("ticks", as_value(0));
	array<Uint8> code;
	push_string(code, "gotA"); push_string(code, "a"); code.push_back(0x1c); code.push_back(0x1d);
	push_string(code, "gotB"); push_string(code, "b"); code.push_back(0x1c); code.push_back(0x1d);
	push_string(code, "ticks"); push_string(code, "ticks"); code.push_back(0x1c); code.push_back(0x50); code.push_back(0x1d);
	code.push_back(0);
	tu_file actions(tu_file::memory_buffer, code.size(), &code[0]);
	stream action_stream(&actions);
	action_buffer buffer;
	buffer.read(&action_stream);
	const array<with_stack_entry> scope;
	smart_ptr<as_as_function> function = new as_as_function(&buffer, env, 0, scope);
	function->set_length(buffer.get_length()); function->add_arg("a"); function->add_arg("b");
	smart_ptr<as_object> receiver = new as_object;
	receiver->set_member("tick", as_value(function.get_ptr()));
	as_value result(99);
	const int refs = receiver->get_ref_count();
	const int locals = env->get_local_frame_top();
	const auto set_timer = [&](const as_value& owner, const char* name, double ms, bool arguments) -> int
	{
		if (arguments) { env->push(20); env->push(10); }
		env->push(ms); env->push(name); env->push(owner);
		timer_setinterval(fn_call(&result, root, env, arguments ? 5 : 3, env->get_top_index()));
		env->drop(arguments ? 5 : 3);
		return int(result.to_number());
	};
	const auto clear_timer = [&](int id)
	{
		env->push(id);
		timer_clearinterval(fn_call(&result, root, env, 1, env->get_top_index()));
		env->drop(1);
	};
	const int id = set_timer(as_value(receiver.get_ptr()), "tick", 125, true);
	check(id == 0, "first global timer ID");
	check(receiver->get_ref_count() == refs && env->get_local_frame_top() == locals, "timer borrows receiver and does not add local barrier");
	const auto read = [&](const char* name) -> double { as_value v; check(root->get_member(name, &v), name); return v.to_number(); };
	update_intervals(1.0f);
	check(read("ticks") == 1 && read("gotA") == 20 && read("gotB") == 10, "script timer and saved argument reversal");
	check(env->get_top_index() == -1, "timer removes saved arguments");
	update_intervals(1.125f);
	check(read("ticks") == 1, "strict extended millisecond comparison");
	update_intervals(std::nextafter(1.125f, 2.0f));
	check(read("ticks") == 2, "clock beyond interval");
	receiver->set_member("tick", as_value(native_timer));
	update_intervals(2.0f);
	check(native_timer_calls == 0 && env->get_top_index() == -1 && read("ticks") == 2, "late lookup accepts but does not invoke C function");
	receiver->set_member("tick", as_value(function.get_ptr()));
	clear_timer(id);
	const double result_after_id = result.to_number();
	clear_timer(1234567);
	check(result.to_number() == result_after_id, "missing timer clear preserves result");
	update_intervals(3.0f);
	check(read("ticks") == 2, "cleared timer does not fire");
	const int next = set_timer(as_value(receiver.get_ptr()), "tick", 0, true);
	check(next == id + 1, "IDs are not reused after clear");
	clear_interval_registry();
	const int after_clear = set_timer(as_value(receiver.get_ptr()), "tick", 0, true);
	check(after_clear == next + 1, "clearing registry does not reset IDs");
	clear_timer(after_clear);
	// A non-object form stores a name but leaves its borrowed receiver null.
	env->push(0); env->push(function.get_ptr());
	timer_setinterval(fn_call(&result, root, env, 2, env->get_top_index()));
	env->drop(2);
	update_intervals(4.0f);
	check(read("ticks") == 2, "non-object timer has no dispatch receiver");
	clear_interval_registry();

	array<Uint8> clear_code;
	// Push argument count 0, look up clearTimerTest, CallFunction, discard result.
	clear_code.push_back(0x96); clear_code.push_back(5); clear_code.push_back(0);
	clear_code.push_back(7); for (int i = 0; i < 4; ++i) clear_code.push_back(0);
	push_string(clear_code, "clearTimerTest"); clear_code.push_back(0x3d); clear_code.push_back(0x17); clear_code.push_back(0);
	tu_file clear_actions(tu_file::memory_buffer, clear_code.size(), &clear_code[0]);
	stream clear_stream(&clear_actions);
	action_buffer clear_buffer;
	clear_buffer.read(&clear_stream);
	smart_ptr<as_as_function> clearing = new as_as_function(&clear_buffer, env, 0, scope);
	clearing->set_length(clear_buffer.get_length());
	root->set_member("clearTimerTest", as_value(clear_current_timer));
	receiver->set_member("tick", as_value(clearing.get_ptr()));
	clearing_timer_id = set_timer(as_value(receiver.get_ptr()), "tick", 0, true);
	update_intervals(5.0f);
	// Retail re-reads the destroyed record's now-empty argument array: no pop runs.
	check(env->get_top_index() == 1, "self-clear retains retail argument-pop quirk");
	env->drop(2);
	update_intervals(6.0f);
	check(env->get_top_index() == -1, "self-cleared timer stays removed");
	clear_interval_registry();
	receiver->set_member("tick", as_value(function.get_ptr()));
	set_timer(as_value(receiver.get_ptr()), "tick", 125, true);
	instance->advance(0.25f);
	check(read("ticks") == 3, "root advance drives global timers before clock increment");
	instance->advance(0.01f);
	check(read("ticks") == 4, "zero previous movie clock repeats first-fire condition");
	instance->advance(0.01f);
	check(read("ticks") == 4, "subsequent root ticks use movie elapsed time");
	clear_interval_registry();
	release_script_objects();

	struct collision { size_t operator()(int) const { return 0; } };
	::hash<int, int, collision> hash;
	hash.set(1, 11); hash.set(2, 22);
	const int size = hash.size();
	hash.invalidate(2);
	check(hash.size() == size && !hash.get(1, NULL) && !hash.get(2, NULL), "invalidation leaves count and broken chain unchanged");
}

static void bounds_checks()
{
	struct definition : character_def
	{
		rect bounds;
		mutable int calls = 0;
		character* mutate = NULL;
		rect get_bound_copy() const override
		{
			++calls;
			if (mutate) { matrix changed; changed.m_[0][2] = 999; mutate->set_matrix(changed); }
			return bounds;
		}
	};
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> movie_definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = movie_definition->create_instance();
	movie* root = instance->get_root_movie();
	character* root_character = root->cast_to_character();
	// The retail definition hierarchy (C1): the bounds slot is pure on character_def (table 0x100DF268
	// +0x20), a movie definition is no character_def, and ref_counted is a shared virtual base.
	static_assert(std::is_abstract<character_def>::value, "character_def +0x20 bounds slot is pure");
	static_assert(!std::is_base_of<character_def, gameswf::movie_definition>::value, "movie definitions are not character_defs");
	static_assert(std::is_base_of<ref_counted, character_def>::value && std::is_base_of<ref_counted, gameswf::movie_definition>::value,
		"both sides reach ref_counted");
	check(dynamic_cast<character_def*>(static_cast<ref_counted*>(movie_definition.get_ptr())) == NULL,
		"the root movie definition is not a character_def");
	const rect empty = root_character->get_bound_copy();
	check(empty.m_x_min == 0 && empty.m_x_max == 0 && empty.m_y_min == 0 && empty.m_y_max == 0, "empty sprite bounds are zero");
	movie_definition_sub* dictionary = static_cast<movie_definition_sub*>(movie_definition.get_ptr());
	smart_ptr<definition> first = new definition;
	first->bounds = rect{ 10, 30, 20, 60 };
	smart_ptr<definition> second = new definition;
	second->bounds = rect{ -5, 5, -10, 10 };
	dictionary->add_character(1, first.get_ptr()); dictionary->add_character(2, second.get_ptr());
	matrix turn;
	turn.m_[0][0] = 0; turn.m_[0][1] = -1; turn.m_[0][2] = 100;
	turn.m_[1][0] = 1; turn.m_[1][1] = 0; turn.m_[1][2] = 200;
	const array<swf_event*> events;
	character* a = root->add_display_object(1, "first", events, 1, false, cxform::identity, turn, 0, 0);
	matrix offset; offset.m_[0][2] = -40; offset.m_[1][2] = -20;
	character* b = root->add_display_object(2, "second", events, 2, false, cxform::identity, offset, 0, 0);
	check(a && b, "bounds fixture display entries");
	check(a->get_bound_copy().m_x_min == 10 && first->calls == 1, "generic forwards definition bounds");
	const rect combined = root_character->get_bound_copy();
	check(combined.m_x_min == -45 && combined.m_x_max == 80 && combined.m_y_min == -30 && combined.m_y_max == 230, "sprite union includes child offsets and rotations");
	check(root_character->get_width() == 6.25f && root_character->get_height() == 13.0f, "sprite pixel extents");
	check(record("sprite_get_width") && record("sprite_get_width")->m_count > 0, "disabled-profiling extent counters");
	root->set_member("_width", as_value(12.5));
	root->set_member("_height", as_value(26));
	as_value width, height;
	root->get_member("_width", &width); root->get_member("_height", &height);
	check(width.to_number() == 12.5 && height.to_number() == 26, "member extents scale the local union");
	root->remove_display_object(2);
	const int removed_calls = second->calls;
	const rect single = root_character->get_bound_copy();
	check(single.m_x_min == 40 && single.m_x_max == 80 && second->calls == removed_calls, "unreferenced child excluded before sweep");
	first->mutate = a;
	const rect before_mutation = root_character->get_bound_copy();
	check(before_mutation.m_x_min == 40 && before_mutation.m_x_max == 80 && a->get_matrix().m_[0][2] == 999, "matrix snapshot precedes child rectangle callback");
	first->mutate = NULL;

	button_character_definition button;
	button.m_menu = false;
	(void)button.get_bound_copy(); // Its four result floats are deliberately not observed.
	button_record entry;
	entry.m_hit_test = true; entry.m_down = false; entry.m_over = false; entry.m_up = true;
	entry.m_character_id = 1; entry.m_character_def = first.get_ptr(); entry.m_button_layer = 1;
	entry.m_button_matrix = turn;
	button.m_button_records.push_back(entry);
	const int before_button = first->calls;
	(void)button.get_bound_copy();
	check(first->calls == before_button + 1, "button queries all non-null definition records");
	// Use an owned definition: button instances retain their definition.
	smart_ptr<button_character_definition> owned_button = new button_character_definition;
	owned_button->m_menu = false; owned_button->m_button_records.push_back(entry);
	smart_ptr<character> button_instance = owned_button->create_character_instance(root, 3);
	const int before_instance = first->calls;
	(void)button_instance->get_bound_copy();
	check(first->calls == before_instance + 1, "button instance forwards bounds virtually");
	root->set_active_mouse_entity(button_instance.get_ptr());
	button_instance = NULL;
	check(root->get_active_mouse_entity() == NULL, "button destruction releases its active capture");
	button_instance = owned_button->create_character_instance(root, 4);
	root->set_active_mouse_entity(a);
	button_instance = NULL;
	check(root->get_active_mouse_entity() == a, "button destruction preserves another character's capture");
	root->set_active_mouse_entity(NULL);
	button_instance = owned_button->create_character_instance(root, 5);
	set_global_root_movie(NULL);
	button_instance = NULL;
	set_global_root_movie(root);
	check(root->get_active_mouse_entity() == NULL, "button destruction accepts an absent published root");
	release_script_objects();
}

static void focus_lifetime_checks()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x1f, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0,
		0x49, 0x09, 1, 0, 0x30, 0x0a, 0, 0xa0, 0, 0, 0,
		0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	movie_definition_sub* dictionary = static_cast<movie_definition_sub*>(definition.get_ptr());
	character_def* text_definition = dictionary->get_character_def(1);
	check(text_definition != NULL, "synthetic edit-text definition");
	const rect text_bounds = text_definition->get_bound_copy();
	check(text_bounds.m_x_min == 0 && text_bounds.m_x_max == 20, "edit-text definition bounds provider");
	smart_ptr<character> field = text_definition->create_character_instance(root, 1);
	check(field->get_bound_copy().m_y_max == 20, "edit-text instance bounds provider");
	field->set_member("_focus", as_value(true));
	check(get_focused_text_input() == dynamic_cast<text_input*>(field.get_ptr()), "focus interface adjustment");
	int destroyed = 0;
	struct lifetime_probe : as_object
	{
		int* destroyed;
		explicit lifetime_probe(int* count) : destroyed(count) {}
		~lifetime_probe() override
		{
			check(get_focused_text_input() != NULL, "focus remains during field member destruction");
			++*destroyed;
		}
	};
	field->set_member("lifetimeProbe", as_value(new lifetime_probe(&destroyed)));
	smart_ptr<as_object> arguments = field->m_arguments;
	arguments->set_member("retained", as_value(73));
	field = NULL;
	check(destroyed == 1 && get_focused_text_input() == NULL, "text-input base destructor clears focus last");
	as_value retained;
	check(arguments->get_member("retained", &retained) && retained.to_number() == 73,
		"native arguments ownership deliberately outlives the text field");
	release_script_objects();
}

static array<int> gc_events;
struct script_probe : as_object_interface
{
	int id;
	int marks = 0;
	int releases = 0;
	int enumerations = 0;
	explicit script_probe(int value) : id(value) {}
	void set_member(const tu_stringi&, const as_value&) override { std::abort(); }
	bool get_member(const tu_stringi&, as_value*) override { std::abort(); }
	movie* to_movie() override { return NULL; }
	void gc_mark() override { ++marks; gc_events.push_back(id); as_object_interface::gc_mark(); }
	void gc_release() override { ++releases; gc_events.push_back(id); as_object_interface::gc_release(); }
	void enumerate(as_environment* env) override { ++enumerations; env->push("probe"); }
};

static void accessor_lifetime_checks()
{
	action_buffer buffer;
	as_environment env;
	const array<with_stack_entry> scope;
	int destroyed = 0;
	struct function_probe : as_as_function
	{
		int* destroyed;
		function_probe(action_buffer* b, as_environment* e, const array<with_stack_entry>& s, int* d)
			: as_as_function(b, e, 0, s), destroyed(d) {}
		~function_probe() override { ++*destroyed; }
	};
	smart_ptr<as_as_function> getter = new function_probe(&buffer, &env, scope, &destroyed);
	smart_ptr<as_as_function> setter = new function_probe(&buffer, &env, scope, &destroyed);
	smart_ptr<as_object> object = new as_object;
	object->add_property("borrowed", as_value(getter.get_ptr()), as_value(setter.get_ptr()));
	check(getter->get_ref_count() == 1 && setter->get_ref_count() == 1,
		"accessor storage borrows both functions");
	as_value found;
	check(object->find_getter("borrowed", &found) && found.to_as_function() == getter.get_ptr()
		&& getter->get_ref_count() == 2, "getter lookup retains only its published value");
	found.set_undefined();
	check(object->find_setter("borrowed", &found) && found.to_as_function() == setter.get_ptr(),
		"setter lookup selects the other borrowed pointer");
	found.set_undefined();

	struct name_probe : as_object
	{
		as_as_function* getter;
		as_as_function* setter;
		name_probe(as_as_function* g, as_as_function* s) : getter(g), setter(s) {}
		const char* get_text_value() const override
		{
			check(getter->get_ref_count() == 2 && setter->get_ref_count() == 2,
				"addProperty captures functions without retaining during name conversion");
			return "converted";
		}
	};
	env.push(as_value(setter.get_ptr()));
	env.push(as_value(getter.get_ptr()));
	env.push(as_value(new name_probe(getter.get_ptr(), setter.get_ptr())));
	as_value result(91);
	as_object_addproperty(fn_call(&result, object.get_ptr(), &env, 3, env.get_top_index()));
	env.drop(3);
	check(result.to_number() == 91 && getter->get_ref_count() == 1 && setter->get_ref_count() == 1,
		"addProperty leaves the result and borrowed ownership unchanged");
	check(object->find_getter("converted", &found) && found.to_as_function() == getter.get_ptr(),
		"addProperty publishes the captured getter under the converted name");
	found.set_undefined();
	object->add_property("borrowed", as_value(), as_value());
	check(!object->find_getter("borrowed", &found) && !object->find_setter("borrowed", &found),
		"null accessor insertion shadows both earlier borrowed functions");
	check(object->m_accessors.size() == 3, "duplicate accessor names occupy distinct hash entries");
	getter = NULL;
	setter = NULL;
	check(destroyed == 2, "accessor storage does not prolong either function's lifetime");
	// "converted" now deliberately contains dangling borrowed pointers. Destruction
	// and issue #7 graph clearing must not inspect or release them.
	object = NULL;
	check(destroyed == 2, "destroying borrowed accessor storage does not release dead functions");

	smart_ptr<as_object> previous = new as_object;
	as_object* middle = new as_object;
	smart_ptr<as_object> next = new as_object;
	previous->m_gc_next = middle;
	middle->m_gc_previous = previous.get_ptr();
	middle->m_gc_next = next.get_ptr();
	next->m_gc_previous = middle;
	delete middle;
	check(previous->m_gc_next == next.get_ptr() && next->m_gc_previous == previous.get_ptr(),
		"common destruction reconnects both collector-list neighbours");
	previous = NULL;
	check(next->m_gc_previous == NULL, "common destruction clears the next node's previous link");
}

static void graph_checks()
{
	smart_ptr<script_probe> own = new script_probe(1);
	smart_ptr<script_probe> explicit_proto = new script_probe(2);
	smart_ptr<script_probe> function_proto = new script_probe(3);
	smart_ptr<script_probe> instance_proto = new script_probe(4);
	smart_ptr<script_probe> ignored = new script_probe(5);
	action_buffer buffer;
	as_environment function_env;
	const array<with_stack_entry> scope;
	smart_ptr<as_as_function> function = new as_as_function(&buffer, &function_env, 0, scope);
	check(as_value(function.get_ptr()).to_object_raw() == NULL
		&& as_value(function.get_ptr()).to_object() == function->m_properties,
		"raw OBJECT extraction excludes native function adapter");
	check(as_value(own.get_ptr()).to_object_raw() == own.get_ptr()
		&& as_value(7).to_object_raw() == NULL, "raw OBJECT extraction is type-only");
	function_env.set_this_object(own.get_ptr());
	function_env.m_super_ctor = function.get_ptr();
	check(function_env.m_this_object == own.get_ptr()
		&& function_env.get_super_constructor() == function.get_ptr(), "borrowed environment fields");
	function->m_properties->set_member("prototype", as_value(function_proto.get_ptr()));
	smart_ptr<as_object> object = new as_object(instance_proto.get_ptr());
	object->set_member("own", as_value(own.get_ptr()));
	object->set_member("__constructor__", as_value(function.get_ptr()));
	object->set_member("__proto__", as_value(explicit_proto.get_ptr()));
	smart_ptr<as_as_function> accessor = new as_as_function(&buffer, &function_env, 0, scope);
	accessor->m_properties->set_member("ignored", as_value(ignored.get_ptr()));
	object->add_property("ignored", as_value(accessor.get_ptr()), as_value());
	advance_gc_mark_generation();
	gc_events.clear();
	object->gc_mark();
	check(gc_events.size() == 4 && gc_events[0] == 1 && gc_events[1] == 2
		&& gc_events[2] == 3 && gc_events[3] == 4, "object GC edge order");
	check(ignored->marks == 0, "accessor-only values not marked");
	object->gc_mark();
	check(gc_events.size() == 4, "generation prevents repeated traversal");
	advance_gc_mark_generation(); object->gc_mark();
	check(gc_events.size() == 8 && own->marks == 2, "new generation traverses again");
	gc_mark_value(as_value()); gc_mark_value(as_value(static_cast<as_object_interface*>(NULL)));
	check(gc_events.size() == 8, "undefined and null object have no GC edge");
	gc_mark_value(as_value(function.get_ptr()));
	check(function_proto->marks == 3, "function traversal has no generation guard");

	as_environment enumeration;
	object->enumerate(&enumeration);
	check(explicit_proto->enumerations == 1 && enumeration.m_stack.size() == 2
		&& enumeration.m_stack[0].to_tu_string() == "probe"
		&& enumeration.m_stack[1].to_tu_string() == "own", "common prototype enumerates before own names");
	gc_events.clear(); object->gc_release();
	check(gc_events.size() == 1 && gc_events[0] == 1 && own->releases == 1,
		"object release visits only member objects");
	check(object->m_members.size() == 0 && object->m_prototype == explicit_proto.get_ptr()
		&& object->m_instance_prototype == instance_proto.get_ptr()
		&& object->m_constructor == function.get_ptr() && object->m_accessors.size() == 1,
		"release retains prototype constructor and accessor storage");
	object->gc_release(); check(own->releases == 1, "release sentinel prevents repetition");
	check(object->m_gc_generation == 0xFEEDBEEFu, "retail release sentinel");

	smart_ptr<as_object> cyclic = new as_object;
	cyclic->set_member("self", as_value(cyclic.get_ptr()));
	advance_gc_mark_generation(); cyclic->gc_mark();
	check(cyclic->m_gc_generation == get_gc_mark_generation(), "mark breaks object cycle");
	cyclic->gc_release(); check(cyclic->m_members.size() == 0, "release breaks member cycle");

	smart_ptr<as_object> list = create_array_object();
	smart_ptr<script_probe> element = new script_probe(6);
	list->set_member("own", as_value(own.get_ptr()));
	array_object_push(list.get_ptr(), as_value(element.get_ptr()));
	array_object_push(list.get_ptr(), as_value(function.get_ptr()));
	advance_gc_mark_generation(); gc_events.clear(); list->gc_mark();
	check(gc_events.size() == 2 && gc_events[0] == 1 && gc_events[1] == 6,
		"array marks inherited members before only object elements");
	gc_events.clear(); list->gc_release();
	check(gc_events.size() == 2 && gc_events[0] == 1 && gc_events[1] == 6
		&& array_object_values(list.get_ptr()).size() == 0, "array release order and clear");

	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	as_environment* env = root->get_action_environment();
	env->m_local_frames.push_back(as_environment::frame_slot("local", as_value(own.get_ptr())));
	root->set_member("member", as_value(element.get_ptr()));
	env->push(ignored.get_ptr()); *env->register_ptr(9) = as_value(ignored.get_ptr());
	advance_gc_mark_generation(); gc_events.clear(); root->gc_mark();
	check(gc_events.size() == 2 && gc_events[0] == 1 && gc_events[1] == 6,
		"sprite visits locals then members without registers or operands");
	check(ignored->marks == 0, "sprite registers and operands omitted");
	const int top = env->m_stack.size(); root->enumerate(env);
	check(env->m_stack.size() == top, "movie enumeration is no-op");
	smart_ptr<as_object> movie_prototype = new as_object;
	movie_prototype->set_member("visible", as_value(7));
	movie_prototype->set_member("__proto__", as_value(root));
	movie_prototype->enumerate(env);
	check(env->m_stack.size() == top + 1 && env->top(0).to_tu_string() == "visible",
		"movie-valued prototype contributes no names");
	env->drop(1);
	// Exercise the actual VM's common-interface dispatch, not an as_object cast.
	unsigned char enumerate_code[] = { 0x55, 0 };
	tu_file action_input(tu_file::memory_buffer, sizeof(enumerate_code), enumerate_code);
	stream action_stream(&action_input); action_buffer enum_buffer; enum_buffer.read(&action_stream);
	env->push(root); enum_buffer.execute(env);
	check(env->m_stack.size() == top + 1 && env->top(0).to_tu_string() == "NULL",
		"Enumerate2 movie dispatch preserves sentinel only");
	gc_events.clear(); root->gc_release();
	check(gc_events.size() == 1 && gc_events[0] == 6 && env->m_variables.size() == 0
		&& env->m_local_frames.size() == 1 && env->m_stack.size() == top + 1,
		"sprite release clears members and retains locals and operands");
	get_global_object()->set_member("gcProbe", as_value(own.get_ptr()));
	const int marks = own->marks; advance_gc_mark_generation(); gc_mark_modules();
	check(own->marks > marks, "module mark traverses independent global module");
	release_script_objects(); action_clear();
}

static void text_graph_checks()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x1f, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0,
		0x49, 0x09, 1, 0, 0x30, 0x0a, 0, 0xa0, 0, 0, 0,
		0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	character_def* text_definition = static_cast<movie_definition_sub*>(definition.get_ptr())->get_character_def(1);
	smart_ptr<character> field = text_definition->create_character_instance(root, 1);
	smart_ptr<script_probe> member = new script_probe(10);
	smart_ptr<script_probe> function_member = new script_probe(11);
	smart_ptr<script_probe> handler = new script_probe(12);
	action_buffer buffer;
	as_environment function_env;
	const array<with_stack_entry> scope;
	smart_ptr<as_as_function> function = new as_as_function(&buffer, &function_env, 0, scope);
	function->m_properties->set_member("edge", as_value(function_member.get_ptr()));
	field->set_member("edge", as_value(member.get_ptr()));
	field->set_member("callback", as_value(function.get_ptr()));
	field->set_member("onChanged", as_value(handler.get_ptr()));
	advance_gc_mark_generation();
	const std::uint32_t old_generation = field->m_gc_generation;
	field->gc_mark();
	check(member->marks == 1 && function_member->marks == 1 && handler->marks == 0,
		"text mark visits member objects/functions and omits handlers");
	check(field->m_gc_generation == old_generation, "text mark leaves own generation untouched");
	field->gc_mark();
	check(member->marks == 2 && function_member->marks == 2, "text mark repeats in the same generation");
	field->m_gc_generation = get_gc_mark_generation();
	field->gc_mark();
	check(member->marks == 2 && function_member->marks == 2, "text matching generation skips traversal");
	field->m_gc_generation = get_gc_release_generation();
	field->gc_release();
	as_value value;
	check(member->releases == 0 && field->get_member("edge", &value), "text release sentinel preserves bag");
	field->m_gc_generation = old_generation;
	field->gc_release();
	check(member->releases == 1 && function_member->releases == 0 && handler->releases == 0,
		"text release visits only member objects");
	check(!field->get_member("edge", &value) && !field->get_member("callback", &value),
		"text release clears both object and function members");
	check(field->m_gc_generation == old_generation && field->get_member("onChanged", &value)
		&& value.to_object_raw() == handler.get_ptr(), "text release leaves generation and handler intact");
	field->gc_release();
	check(member->releases == 1, "text empty bag releases no additional edges");
	release_script_objects();
}

static void text_property_checks(bool invalid_tab = false)
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x1f, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0,
		0x49, 0x09, 1, 0, 0x30, 0x0a, 0, 0xa0, 0, 0, 0,
		0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	character_def* text_definition = static_cast<movie_definition_sub*>(definition.get_ptr())->get_character_def(1);
	smart_ptr<character> field = text_definition->create_character_instance(root, 1);
	if (invalid_tab)
	{
		field->set_member("tabIndex", as_value(-1));
		check(false, "negative Tab setter unexpectedly returned");
	}
	field->set_text_value("narrow text");
	const Uint16 wide[] = { 'w', 'i', 'd', 'e', 0 };
	smart_ptr<as_object_interface> output_string = create_dice_string_object(wide, 4);
	as_value value(output_string.get_ptr());
	check(field->get_member("text", &value) && value.to_object_raw() == output_string.get_ptr(),
		"text getter preserves the supplied String object identity");
	check(std::strcmp(output_string->get_text_value(), "narrow text") == 0
		&& output_string->get_wide_text_value()[0] == 0,
		"text getter fills narrow payload and clears wide payload");
	as_value length;
	check(output_string->get_member("length", &length) && length.to_number() == 11,
		"in-place String update exposes narrow length");
	value.set_double(77);
	check(field->get_member("text", &value) && value.get_type() == as_value::STRING
		&& std::strcmp(value.to_string(), "narrow text") == 0, "text getter replaces scalar output");
	action_buffer buffer;
	as_environment env;
	smart_ptr<as_as_function> function = new as_as_function(&buffer, &env, 0, array<with_stack_entry>());
	value.set_as_as_function(function.get_ptr());
	check(field->get_member("text", &value) && value.get_type() == as_value::STRING,
		"text getter does not use a function's native object adapter");
	matrix transform;
	transform.m_[0][2] = 1.1f; transform.m_[1][2] = -1.1f;
	field->set_matrix(transform);
	check(field->get_member("_x", &value) && value.to_number() == double(1.1f) * double(0.05f),
		"text X getter avoids an intermediate float store");
	check(field->get_member("_y", &value) && value.to_number() == double(-1.1f) * double(0.05f),
		"text Y getter avoids an intermediate float store");
	cxform color;
	color.m_[3][0] = 1.1f;
	field->set_cxform(color);
	check(field->get_member("_alpha", &value) && value.to_number() == double(1.1f) * 100.0,
		"text alpha getter publishes a double product");
	const double alphas[] = { 1.0000001, -1, 0, 100, 300,
		std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity() };
	for (double alpha : alphas)
	{
		field->set_member("_alpha", as_value(alpha));
		const double product = alpha * double(0.01f);
		const float upper = product < 1.0 ? float(product) : 1.0f;
		const float expected = 0.0f < upper ? upper : 0.0f;
		check(field->get_cxform().m_[3][0] == expected, "text alpha setter multiply/narrow/clamp order");
	}
	check(field->get_member("textColor", &value) && value.to_number() == -16777216.0,
		"text color retains signed alpha-bearing dword");
	check(field->get_member("borderColor", &value) && value.to_number() == -16777216.0,
		"border color includes alpha");
	check(field->get_member("backgroundColor", &value) && value.to_number() == -1.0,
		"background color includes alpha");
	check(field->get_member("_width", &value) && value.to_number() == 20.0 * double(0.05f), "text width bridge yields pixels");
	check(field->get_member("_height", &value) && value.to_number() == 20.0 * double(0.05f), "text height bridge yields pixels");
	check(field->get_member("_parent", &value) && value.to_object_raw() == root, "text parent adjusts native interface");
	// set_int has the shipped duplicate release; use a single release to reset
	// this OBJECT result without consuming the movie's own reference.
	value.set_double(77);
	check(!field->get_member("tabIndex", &value) && value.to_number() == 77,
		"negative tab index leaves output unchanged");
	check(!field->get_member("TEXT", &value) && value.to_number() == 77,
		"text property names remain case sensitive");
	check(!field->get_member("absent", &value) && value.to_number() == 77,
		"missing environment property leaves output unchanged");
	field->set_member("tabIndex", as_value(4294967297.75));
	check(field->get_member("tabIndex", &value) && value.to_number() == 1,
		"text tab index keeps the low dword of qword truncation");
	smart_ptr<character> from = text_definition->create_character_instance(root, 2);
	smart_ptr<character> next = text_definition->create_character_instance(root, 3);
	field->set_member("tabIndex", as_value(2));
	from->set_member("tabIndex", as_value(1));
	next->set_member("tabIndex", as_value(3));
	from->set_member("_focus", as_value(true));
	struct tab_conversion : as_object
	{
		text_input* expected;
		int* calls;
		tab_conversion(text_input* target, int* count) : expected(target), calls(count) {}
		const char* get_text_value() const override
		{
			++*calls;
			get_focused_text_input()->on_key(9);
			check(get_focused_text_input() == expected, "Tab entry is removed before object-to-number conversion");
			return "4.75";
		}
	};
	int conversions = 0;
	field->set_member("tabIndex", as_value(new tab_conversion(dynamic_cast<text_input*>(next.get_ptr()), &conversions)));
	check(conversions == 1 && field->get_member("tabIndex", &value) && value.to_number() == 4,
		"text Tab conversion runs once and truncates the qword result");
	next->set_member("_focus", as_value(false));
	field->set_member("maxChars", as_value(std::numeric_limits<double>::quiet_NaN()));
	check(field->get_member("maxChars", &value) && value.to_number() == -2147483648.0,
		"text maximum character count uses integer indefinite");
	field->set_member("backgroundColor", as_value(-1));
	check(field->get_member("backgroundColor", &value) && value.to_number() == -1,
		"text background color writes low RGB bytes and full alpha");
	field->set_member("border", as_value(true));
	check(field->get_member("border", &value) && value.to_bool(), "text border round trip");
	field->set_member("password", as_value(true));
	check(field->get_member("password", &value) && value.to_bool(), "text password round trip");
	field->set_member("_visible", as_value(false));
	check(field->get_member("_visible", &value) && !value.to_bool(), "text visibility round trip");
	field->set_member("custom", as_value(19));
	check(field->get_member("custom", &value) && value.to_number() == 19, "text falls back to environment bag");
	check(field->get_member("restrict", &value) && std::strcmp(value.to_string(), "") == 0,
		"restrict getter publishes the empty string");
	check(field->get_member("_focus", &value) && !value.to_bool(), "text focus compares the input interface");
	const char* duplicate_release_names[] = { "textColor", "borderColor", "backgroundColor", "textHeight", "tabIndex" };
	for (const char* name : duplicate_release_names)
	{
		smart_ptr<as_object> probe = new as_object;
		// [TEMP] Supply the second reference consumed by the shipped getter.
		probe->add_ref();
		as_value output(probe.get_ptr());
		check(field->get_member(name, &output) && probe->get_ref_count() == 1,
			"integer text getter repeats release of the old result");
	}
	{
		smart_ptr<as_object> probe = new as_object;
		as_value output(probe.get_ptr());
		check(field->get_member("maxChars", &output) && probe->get_ref_count() == 1,
			"maxChars getter assigns its temporary with one release");
	}
	release_script_objects();
}

static void matrix_interface_checks()
{
	matrix left;
	left.m_[0][0] = 4097; left.m_[0][1] = -4096;
	matrix right;
	right.m_[0][0] = 4097; right.m_[1][0] = 4098;
	left.concatenate(right);
	check(left.m_[0][0] == 1, "matrix concatenate retains precision across cancelling products");
	matrix square;
	square.m_[0][0] = 2; square.m_[0][1] = 1; square.m_[0][2] = 3;
	square.m_[1][0] = 1; square.m_[1][1] = 1; square.m_[1][2] = 4;
	matrix same = square;
	same.concatenate(same);
	check(same.m_[0][0] == 5 && same.m_[0][1] == 3 && same.m_[0][2] == 13
		&& same.m_[1][0] == 3 && same.m_[1][1] == 2 && same.m_[1][2] == 11,
		"matrix concatenate snapshots aliased input before output");
	point input; input.m_x = 5; input.m_y = 7;
	point output;
	square.transform_by_inverse(&output, input);
	check(output.m_x == -1 && output.m_y == 4, "inverse point coordinates");
	square.transform_by_inverse(&input, input);
	check(input.m_x == -1 && input.m_y == 10, "inverse point preserves first-coordinate alias effect");
}

static array<tu_string> init_messages;
static array<tu_string> timeline_messages;
static movie_definition_sub* init_definition;
static int init_count_before;
static int parse_disassembly_lines;
static void init_action_log(bool error, const char* message)
{
	if (std::strcmp(message, "   0\t") == 0) ++parse_disassembly_lines;
	if (std::strstr(message, "do_action_loader") || std::strstr(message, "-- actions in frame"))
		timeline_messages.push_back(tu_string(message));
	if (std::strstr(message, "do_init_action_loader") || std::strstr(message, "init actions in sprite"))
	{
		check(!error && init_definition->get_init_actions(0)->size() == init_count_before,
			"init-action logging precedes definition append");
		init_messages.push_back(tu_string(message));
	}
}

static void init_action_interface_checks()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	init_definition = static_cast<movie_definition_sub*>(definition.get_ptr());
	register_log_callback(init_action_log);
	for (int flags = 0; flags < 4; ++flags)
	{
		set_verbose_loader((flags & 1) != 0);
		set_verbose_parse((flags & 2) != 0);
		set_verbose_action(true);
		init_messages.clear();
		parse_disassembly_lines = 0;
		init_count_before = init_definition->get_init_actions(0)->size();
		// Tag 59 with an absent dictionary ID and one End action byte.
		unsigned char tag_bytes[] = { 0xc3, 0x0e, 0xff, 0xff, 0 };
		tu_file tag_input(tu_file::memory_buffer, sizeof(tag_bytes), tag_bytes);
		stream tag(&tag_input);
		check(tag.open_tag() == 59, "synthetic init-action tag");
		do_init_action_loader(&tag, 59, init_definition);
		const array<execute_tag*>* actions = init_definition->get_init_actions(0);
		check(actions->size() == init_count_before + 1 && actions->back()->is_action_tag(),
			"init action appends despite missing unused sprite ID");
		check(tag.get_position() == tag.get_tag_end_position(), "init-action buffer consumes payload");
		check(init_messages.size() == ((flags & 1) != 0) + ((flags & 2) != 0),
			"loader/parse verbosity gates are independent of action verbosity");
		check(parse_disassembly_lines == ((flags & 2) != 0), "buffer disassembly uses parse verbosity");
		int index = 0;
		if (flags & 1) check(init_messages[index++] == "tag 59: do_init_action_loader\n", "init loader message");
		if (flags & 2) check(init_messages[index++] == "-- init actions in sprite 65535\n", "init action message");
		tag.close_tag();

		timeline_messages.clear(); parse_disassembly_lines = 0;
		unsigned char timeline_bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x17, 0, 0, 0,
			0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 1, 3, 0, 0x40, 0, 0, 0 };
		tu_file timeline_input(tu_file::memory_buffer, sizeof(timeline_bytes), timeline_bytes);
		smart_ptr<movie_definition> timeline = create_movie_no_recurse(&timeline_input,
			DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
		const array<execute_tag*>& playlist = static_cast<movie_definition_sub*>(timeline.get_ptr())->get_playlist(0);
		check(playlist.size() == 1 && playlist[0]->is_action_tag(), "timeline loader publishes parsed action tag");
		check(timeline_messages.size() == ((flags & 1) != 0) + ((flags & 2) != 0),
			"timeline loader keeps loader and parse log gates independent");
		index = 0;
		if (flags & 1) check(timeline_messages[index++] == "tag 12: do_action_loader\n", "timeline loader message");
		if (flags & 2) check(timeline_messages[index++] == "-- actions in frame 0\n", "timeline frame message");
		check(parse_disassembly_lines == ((flags & 2) != 0), "timeline action disassembly gate");
	}
	set_verbose_loader(false); set_verbose_action(false); set_verbose_parse(false);
	set_verbose_gc(true); check(s_verbose_gc, "GC diagnostic flag setter");
	set_verbose_gc(false);
	register_log_callback(NULL); init_definition = NULL;
}

static array<int> drag_conversions;
struct drag_number : as_object
{
	int id;
	const char* text;
	movie* target;
	movie* parent;
	as_environment* env;
	drag_number(int i, const char* value, movie* t, movie* p, as_environment* e)
		: id(i), text(value), target(t), parent(p), env(e) {}
	const char* get_text_value() const override
	{
		drag_conversions.push_back(id);
		check(env->m_stack.size() == 7, "drag converts bounds before removing operands");
		if (id == 1)
		{
			matrix changed;
			changed.m_[0][2] = 1000; changed.m_[1][2] = 1200;
			target->cast_to_character()->set_matrix(changed);
			changed.m_[0][2] = 900; changed.m_[1][2] = 1000;
			parent->cast_to_character()->set_matrix(changed);
		}
		return text;
	}
};
static int init_queue_calls, timeline_queue_calls;
static void observe_init_queue(const fn_call&)
{
	++init_queue_calls;
	check(timeline_queue_calls == 0, "definition init does not drain queued timeline actions");
}
static void observe_timeline_queue(const fn_call&) { ++timeline_queue_calls; }
struct timer_character : generic_character
{
	float timer = 0;
	timer_character(character_def* d, movie* p) : generic_character(d, p, 2) {}
	float get_timer() const override { return timer; }
};

static void vm_interface_checks()
{
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	as_environment* env = root->get_action_environment();
	root->set_member("initQueueProbe", as_value(observe_init_queue));
	root->set_member("timelineQueueProbe", as_value(observe_timeline_queue));
	array<Uint8> queued_code;
	push_string(queued_code, "0"); push_string(queued_code, "timelineQueueProbe");
	queued_code.push_back(0x3d); queued_code.push_back(0x17); queued_code.push_back(0);
	tu_file queued_input(tu_file::memory_buffer, queued_code.size(), &queued_code[0]);
	stream queued_stream(&queued_input); action_buffer queued; queued.read(&queued_stream);
	root->add_action_buffer(&queued);
	array<Uint8> init_code;
	push_string(init_code, "0"); push_string(init_code, "initQueueProbe");
	init_code.push_back(0x3d); init_code.push_back(0x17); init_code.push_back(0);
	array<Uint8> tag_bytes;
	const unsigned tag = (59 << 6) | (init_code.size() + 2);
	tag_bytes.push_back(Uint8(tag)); tag_bytes.push_back(Uint8(tag >> 8));
	tag_bytes.push_back(0); tag_bytes.push_back(0);
	for (int i = 0; i < init_code.size(); ++i) tag_bytes.push_back(init_code[i]);
	tu_file init_input(tu_file::memory_buffer, tag_bytes.size(), &tag_bytes[0]);
	stream init_stream(&init_input);
	check(init_stream.open_tag() == 59, "synthetic queued init tag");
	do_init_action_loader(&init_stream, 59, static_cast<movie_definition_sub*>(definition.get_ptr()));
	init_stream.close_tag();
	init_queue_calls = timeline_queue_calls = 0;
	root->on_event_load();
	check(init_queue_calls == 1 && timeline_queue_calls == 0,
		"definition init executes directly before frame-zero queue discard");
	root->on_event_load();
	check(init_queue_calls == 1, "definition init clears its pending flag after execution");

	smart_ptr<shape_character_def> shape = new shape_character_def;
	smart_ptr<character> child = shape->create_character_instance(root, 1);
	matrix parent_matrix; parent_matrix.m_[0][2] = 100; parent_matrix.m_[1][2] = 200;
	root->cast_to_character()->set_matrix(parent_matrix);
	matrix child_matrix; child_matrix.m_[0][2] = 40; child_matrix.m_[1][2] = 60;
	child->set_matrix(child_matrix);
	instance->notify_mouse_state(10, 20, 0);
	smart_ptr<drag_number> x0 = new drag_number(2, "1", child.get_ptr(), root, env);
	smart_ptr<drag_number> y0 = new drag_number(1, "2", child.get_ptr(), root, env);
	smart_ptr<drag_number> x1 = new drag_number(4, "3", child.get_ptr(), root, env);
	smart_ptr<drag_number> y1 = new drag_number(3, "4", child.get_ptr(), root, env);
	env->push(x0.get_ptr()); env->push(y0.get_ptr()); env->push(x1.get_ptr()); env->push(y1.get_ptr());
	env->push(true); env->push(false); env->push(child.get_ptr());
	unsigned char drag_code[] = { 0x27, 0 };
	tu_file drag_input(tu_file::memory_buffer, sizeof(drag_code), drag_code);
	stream drag_stream(&drag_input); action_buffer drag; drag.read(&drag_stream);
	drag.execute(env);
	movie::drag_state state;
	root->get_drag_state(&state);
	check(drag_conversions.size() == 4 && drag_conversions[0] == 1 && drag_conversions[1] == 2
		&& drag_conversions[2] == 3 && drag_conversions[3] == 4, "drag converts each Y before X");
	check(state.m_character == child.get_ptr() && state.m_offset_x == 60 && state.m_offset_y == 140,
		"drag captures offset before reentrant bounds conversion changes matrices");
	check(state.m_bound && state.m_bound_x0 == 120 && state.m_bound_y0 == 240
		&& state.m_bound_x1 == 160 && state.m_bound_y1 == 280,
		"drag uses saved parent transform and transforms bounds exactly once");
	env->m_stack.clear();
	state.m_offset_x = 17; state.m_offset_y = -19;
	root->set_drag_state(state);
	movie::drag_state copied; root->get_drag_state(&copied);
	check(copied.m_offset_x == 17 && copied.m_offset_y == -19 && copied.m_bound_x0 == 120,
		"completed drag publication performs no coordinate conversion");
	root->stop_drag();
	smart_ptr<timer_character> timed = new timer_character(shape.get_ptr(), root);
	as_environment timer_env; timer_env.set_target(timed.get_ptr());
	unsigned char timer_code[] = { 0x34, 0x3e, 0 };
	tu_file timer_input(tu_file::memory_buffer, sizeof(timer_code), timer_code);
	stream timer_stream(&timer_input); action_buffer timer; timer.read(&timer_stream);
	for (float seconds : { -0.0f, -0.001f, 0.001f, 12345.678f,
		std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(),
		std::numeric_limits<float>::quiet_NaN() })
	{
		timed->timer = seconds;
		as_value result;
		timer.execute(&timer_env, 0, timer.get_length(), &result, array<with_stack_entry>(), false);
		const float product = float(static_cast<long double>(seconds) * 1000.0L);
		const float expected = float(std::floor(double(product)));
		check(result.get_type() == as_value::NUMBER
			&& (std::isnan(expected) ? std::isnan(result.to_number())
				: result.to_number() == expected && std::signbit(result.to_number()) == std::signbit(expected)),
			"VM timer preserves product store, double floor and float publication");
		check(timer_env.m_stack.size() == 0, "VM timer restores operand count");
	}
}

static void container_interface_checks()
{
	path original(2, 3, 4, 5, 6);
	original.m_new_shape = true;
	original.m_edges.push_back(edge(7, 8, 9, 10));
	path path_copy;
	path_copy = original;
	check(path_copy.m_ax == 2 && path_copy.m_ay == 3 && path_copy.m_fill0 == 4
		&& path_copy.m_fill1 == 5 && path_copy.m_line == 6 && path_copy.m_new_shape,
		"path assignment retains its style/start fields and final flag");
	check(&path_copy.m_edges[0] != &original.m_edges[0] && path_copy.m_edges[0].m_ax == 9,
		"path assignment owns an independent edge array");
	original.m_edges[0].m_ax = 99;
	check(path_copy.m_edges[0].m_ax == 9, "path copy survives source mutation");
	smart_ptr<bitmap_info> bitmap = new bitmap_info;
	array<texture_glyph> glyphs;
	glyphs.resize(3);
	check(!glyphs[0].is_renderable() && glyphs[0].m_uv_origin.m_x == 0
		&& glyphs[0].m_uv_origin.m_y == 0, "glyph resize constructs origin and null bitmap");
	glyphs[0].set_bitmap_info(bitmap.get_ptr());
	glyphs[2].set_bitmap_info(bitmap.get_ptr());
	check(bitmap->get_ref_count() == 3, "glyph elements retain separate bitmap references");
	glyphs.resize(1);
	check(bitmap->get_ref_count() == 2 && glyphs[0].is_renderable(), "glyph shrink releases removed element");
	glyphs.resize(17);
	check(bitmap->get_ref_count() == 2 && !glyphs[16].is_renderable(), "glyph growth relocates old ownership once");
	glyphs.clear();
	check(bitmap->get_ref_count() == 1 && glyphs.size() == 0, "glyph clear releases last element");
	array<as_value> values;
	values.push_back(as_value("a long independent string for the array copy"));
	smart_ptr<as_object> object = new as_object;
	values.push_back(as_value(object.get_ptr()));
	array<as_value> copied;
	copied = values;
	check(object->get_ref_count() == 3 && copied[0].to_tu_string() == values[0].to_tu_string(),
		"value array assignment copies owned elements");
	values.clear();
	check(object->get_ref_count() == 2 && copied[0].get_type() == as_value::STRING,
		"copied array survives original clear");
	copied.clear(); check(object->get_ref_count() == 1, "copied array clear releases value");
	::hash<event_id, as_value>::entry event_entry(event_id(event_id::KEY_PRESS, key::A),
		as_value(object.get_ptr()), -1, 19);
	::hash<event_id, as_value>::entry copied_entry(event_entry);
	check(copied_entry.first == event_entry.first && copied_entry.m_hash_value == 19
		&& copied_entry.m_next_in_chain == -1 && copied_entry.second.to_object() == object.get_ptr(),
		"event entry copy preserves both key bytes and owned value");
	stringi_hash<smart_ptr<as_object> > named;
	named.set(tu_stringi("a long independent key for hash ownership"), object);
	smart_ptr<as_object> result;
	check(named.get(tu_stringi("a long independent key for hash ownership"), &result)
		&& result == object.get_ptr(), "hash lookup copies held output");
	check(named.get(tu_stringi("a long independent key for hash ownership"), NULL),
		"hash lookup permits null output");
	check(!named.get(tu_stringi("absent"), &result) && result == object.get_ptr(),
		"failed hash lookup preserves output");
	const int refs = object->get_ref_count(); result = result;
	check(object->get_ref_count() == refs, "held-pointer self assignment retains ownership");
	stringi_hash<smart_ptr<as_object> >::iterator it = named.begin();
	check(it->second == object.get_ptr(), "iterator dereferences live entry");
	++it; ++it;
	check(it == named.end(), "iterator skips empty buckets and stays at end");
}

static as_as_function* expected_class_super;
static int construct_calls;
static void observe_construct(const fn_call& fn)
{
	++construct_calls;
	as_value result;
	check(fn.this_ptr->get_member("super", &result)
		&& result.to_as_function() == expected_class_super,
		"CONSTRUCT receives the prototype's published super context");
}

static as_object_interface* command_receiver;
static int command_refs;
static int command_calls;
static void observe_command(const fn_call& fn)
{
	++command_calls;
	check(fn.this_ptr == command_receiver && fn.this_ptr->get_ref_count() == command_refs + 1,
		"command parser retains the exact receiver during callback");
	check(fn.nargs == 2 && std::strcmp(fn.arg(0).to_string(), "alpha,beta") == 0
		&& fn.arg(1).to_number() == -2.5, "command forwards quoted and numeric arguments");
	fn.result->set_int(123);
}

static void observe_unicode_command(const fn_call& fn)
{
	check(fn.this_ptr == command_receiver && fn.this_ptr->get_ref_count() == command_refs + 1,
		"UTF-16 command retains receiver");
	const char* text = fn.arg(0).to_string();
	check(fn.nargs == 1 && std::strcmp(text, "\xc3\xa9\xed\xa0\xbd\xed\xb8\x80") == 0,
		"UTF-16 command encodes BMP and surrogate code units separately");
	fn.result->set_bool(true);
}

static void character_interface_checks()
{
	array<int> destruction_order;
	struct environment_probe : as_object
	{
		array<int>* order;
		int id;
		environment_probe(array<int>* values, int identity) : order(values), id(identity) {}
		~environment_probe() override { order->push_back(id); }
	};
	{
		as_environment dying;
		dying.m_local_frames.push_back(as_environment::frame_slot("local",
			as_value(new environment_probe(&destruction_order, 1))));
		dying.set_member("member", as_value(new environment_probe(&destruction_order, 2)));
		*dying.register_ptr(0) = as_value(new environment_probe(&destruction_order, 3));
		*dying.register_ptr(255) = as_value(new environment_probe(&destruction_order, 4));
		dying.push(new environment_probe(&destruction_order, 5));
	}
	check(destruction_order.size() == 5 && destruction_order[0] == 1
		&& destruction_order[1] == 2 && destruction_order[2] == 4
		&& destruction_order[3] == 3 && destruction_order[4] == 5,
		"environment releases locals, members, reverse registers, then operands");
	as_environment fresh;
	check(fresh.m_stack.size() == 0 && fresh.m_registers.size() == 256
		&& fresh.m_local_frames.size() == 0 && fresh.m_variables.size() == 0,
		"environment constructs full register bank and empty dynamic storage");
	for (int i = 0; i < 256; ++i)
		check(fresh.m_registers[i].get_type() == as_value::UNDEFINED,
			"every initial register is undefined");
	// One dictionary sprite and a one-frame root; no imported resources or device.
	unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 30, 0, 0, 0,
		0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0,
		0xc8, 0x09, 1, 0, 1, 0, 0x40, 0, 0, 0, 0x40, 0, 0, 0 };
	tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
	smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
		DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
	smart_ptr<movie_interface> instance = definition->create_instance();
	movie* root = instance->get_root_movie();
	root->set_member("commandProbe", as_value(observe_command));
	as_value parent_result(99);
	check(root->get_member("_parent", &parent_result)
		&& parent_result.get_type() == as_value::OBJECT && parent_result.to_object_raw() == NULL,
		"parentless sprite returns null OBJECT rather than falling through");
	command_receiver = root; command_refs = root->get_ref_count(); command_calls = 0;
	const char expression[] = "commandProbe('alpha,beta', -2.5)";
	const Uint16 wide_expression[] = { 'c','o','m','m','a','n','d','P','r','o','b','e',
		'(', '\'', 'a','l','p','h','a',',','b','e','t','a','\'',',',' ','-','2','.','5',')',0 };
	check(std::strcmp(instance->call_method(expression), "123") == 0,
		"root wrapper forwards narrow command and result");
	check(std::strcmp(instance->call_method(wide_expression), "123") == 0,
		"root wrapper forwards UTF-16 command and result");
	check(std::strcmp(root->call_method(expression), "123") == 0
		&& std::strcmp(root->call_method(wide_expression), "123") == 0,
		"sprite command overloads call both production parsers");
	check(command_calls == 4 && root->get_ref_count() == command_refs
		&& root->get_action_environment()->m_stack.size() == 0,
		"command calls release receiver and remove parsed arguments");
	root->set_member("commandUnicode", as_value(observe_unicode_command));
	const Uint16 unicode_expression[] = { 'c','o','m','m','a','n','d','U','n','i','c','o','d','e',
		' ', '\'', 0xE9, 0xD83D, 0xDE00, '\'', 0 };
	check(std::strcmp(instance->call_method(unicode_expression), "true") == 0,
		"UTF-16 command uses recovered base construction and result conversion");
	smart_ptr<character_def> plain_definition = new shape_character_def;
	smart_ptr<character> plain = plain_definition->create_character_instance(root, 2);
	check(plain->call_method(expression) == NULL && plain->call_method(wide_expression) == NULL,
		"non-script characters return null for both command widths");
	smart_ptr<character> other_mask = plain_definition->create_character_instance(root, 3);
	character* root_character = root->cast_to_character();
	root_character->set_mask(plain.get_ptr());
	check(root_character->get_mask() == plain.get_ptr() && plain->is_mask_only(),
		"mask setter publishes the character and sets its mask-only flag");
	root_character->set_mask(other_mask.get_ptr()); root_character->set_mask(NULL);
	check(root_character->get_mask() == NULL && plain->is_mask_only() && other_mask->is_mask_only(),
		"mask replacement and clear retain both previous mask-only flags");
	as_value arguments;
	check(root->get_member("arguments", &arguments) && arguments.to_object_raw() != NULL,
		"root arguments exposes an Array");
	as_object_interface* original_arguments = arguments.to_object_raw();
	check(array_object_values(original_arguments).size() == 0,
		"character arguments starts empty");
	original_arguments->set_member("0", as_value(42));
	root->set_member("arguments", as_value(99));
	check(root->get_member("arguments", &arguments)
		&& arguments.to_object_raw() == original_arguments
		&& array_object_values(original_arguments).size() == 1
		&& array_object_values(original_arguments)[0].to_number() == 42,
		"arguments identity and dense values survive an environment-name assignment");
	as_value stored;
	check(root->get_action_environment()->m_variables.get("arguments", &stored)
		&& stored.to_number() == 99, "arguments assignment retains ordinary member write");
	action_buffer empty;
	const array<with_stack_entry> scope;
	smart_ptr<as_as_function> parent_ctor = new as_as_function(&empty, NULL, 0, scope);
	array<Uint8> code;
	push_string(code, "seenSuper"); push_string(code, "super");
	code.push_back(0x1c); code.push_back(0x1d); code.push_back(0);
	tu_file action_input(tu_file::memory_buffer, code.size(), &code[0]);
	stream instructions(&action_input); action_buffer body; body.read(&instructions);
	smart_ptr<as_as_function> cls = new as_as_function(&body, NULL, 0, scope);
	cls->set_length(body.get_length());
	as_value prototype;
	cls->m_properties->get_member("prototype", &prototype);
	prototype.to_object_raw()->set_member("__constructor__", as_value(parent_ctor.get_ptr()));
	movie_definition_sub* dictionary = static_cast<movie_definition_sub*>(definition.get_ptr());
	character_def* sprite = dictionary->get_character_def(1);
	check(sprite != NULL, "synthetic sprite definition exists");
	sprite->m_registered_class = cls.get_ptr(); cls->add_ref();
	smart_ptr<character> child = sprite->create_character_instance(root, 1);
	as_value child_arguments;
	check(child->get_member("arguments", &child_arguments)
		&& child_arguments.to_object_raw() != original_arguments
		&& array_object_values(child_arguments.to_object_raw()).size() == 0,
		"each character owns a distinct arguments Array");
	expected_class_super = parent_ctor.get_ptr(); construct_calls = 0;
	child->set_event_handler(event_id::CONSTRUCT, as_value(observe_construct));
	set_call_super(NULL);
	child->on_event_load();
	check(construct_calls == 1, "class loading sends CONSTRUCT once");
	as_value seen;
	check(child->get_member("seenSuper", &seen)
		&& seen.to_as_function() == parent_ctor.get_ptr(),
		"registered class constructor sees the same published super context");
	check(child->get_member("arguments", &child_arguments)
		&& array_object_values(child_arguments.to_object_raw()).size() == 0,
		"class binding retains its character's arguments Array");
	as_as_function* borrowed_super = new as_as_function(&empty, NULL, 0, scope);
	as_value super_result(borrowed_super);
	child->get_action_environment()->m_super_ctor = borrowed_super;
	check(borrowed_super->get_ref_count() == 1, "super output is the sole owning reference");
	check(child->get_member("super", &super_result) && super_result.to_as_function() == borrowed_super,
		"sprite super getter publishes a script-function temporary");
	check(child->get_member("super", &super_result) && borrowed_super->get_ref_count() == 1,
		"aliased super result keeps function ownership balanced");
	child->get_action_environment()->m_super_ctor = parent_ctor.get_ptr();

	struct text_sink : generic_character
	{
		as_value* source;
		tu_string received;
		text_sink(character_def* def, movie* parent, int id, as_value* value)
			: generic_character(def, parent, id), source(value) {}
		const char* get_text_name() const override
		{
			check(source->m_string_value == "12", "text converted before child-name dispatch");
			return "textAssignment";
		}
		void set_text_value(const char* value) override
		{
			received = value;
			source->m_number_value = 99;
		}
	};
	struct sink_definition : character_def
	{
		as_value* source;
		explicit sink_definition(as_value* value) : source(value) {}
		rect get_bound_copy() const override { rect empty = { 0, 0, 0, 0 }; return empty; }   // a concrete definition answers +0x20
		character* create_character_instance(movie* parent, int id) override
		{
			return new text_sink(this, parent, id, source);
		}
	};
	as_value text_source(12);
	smart_ptr<sink_definition> sink_def = new sink_definition(&text_source);
	dictionary->add_character(2, sink_def.get_ptr());
	const array<swf_event*> no_events;
	text_sink* first = static_cast<text_sink*>(root->add_display_object(2, "firstSink", no_events, 2,
		false, cxform::identity, matrix::identity, 0, 0));
	text_sink* second = static_cast<text_sink*>(root->add_display_object(2, "secondSink", no_events, 3,
		false, cxform::identity, matrix::identity, 0, 0));
	root->set_member("textAssignment", text_source);
	check(first->received == "12" && second->received == "12" && text_source.to_number() == 99,
		"all matching text children receive the single pre-scan conversion");
	release_script_objects();
	check(array_object_values(original_arguments).size() == 0,
		"native teardown clears persistent arguments storage");
	action_clear(); expected_class_super = NULL;
}

int main(int argc, char** argv)
{
	if (argc == 2 && std::strcmp(argv[1], "--new-method-c-function") == 0)
	{
		unsigned char code[] = { 0x53, 0 };
		tu_file input(tu_file::memory_buffer, sizeof(code), code);
		stream instructions(&input); action_buffer buffer; buffer.read(&instructions);
		smart_ptr<as_object> receiver = new as_object;
		receiver->set_member("factory", as_value(native_timer));
		as_environment env;
		env.push(0); env.push(receiver.get_ptr()); env.push("factory");
		buffer.execute(&env);
		return 0;
	}
	if (argc == 2 && std::strcmp(argv[1], "--missing-async-input") == 0)
	{
		set_async_key_state_callback(NULL);
		return is_key_down(13) ? 1 : 0;   // no host adapter: Enter reads up
	}
	if (argc == 2 && std::strcmp(argv[1], "--text-tab-invalid") == 0)
	{
		text_property_checks(true);
		return 1;
	}
	if (argc == 2 && (std::strcmp(argv[1], "--enumerate-null") == 0
		|| std::strcmp(argv[1], "--enumerate-function") == 0))
	{
		unsigned char code[] = { 0x55, 0 };
		tu_file input(tu_file::memory_buffer, sizeof(code), code);
		stream instructions(&input); action_buffer buffer; buffer.read(&instructions);
		as_environment env;
		if (std::strcmp(argv[1], "--enumerate-function") == 0)
		{
			const array<with_stack_entry> scope;
			env.push(new as_as_function(&buffer, &env, 0, scope));
		}
		else env.push(7);
		buffer.execute(&env);
		std::exit(0);
	}
	set_profiling(false);
	const int initial_records = get_profile_records().size();
	render::end_display();
	check(get_profile_records().size() == initial_records, "absent handler does not register");
	set_profiling(true);
	render::end_display();
	check(get_profile_records().size() == initial_records, "profiled absent handler does not register");
	observer handler;
	set_render_handler(&handler);
	render::set_depth(-2147483647);
	check(handler.depth == -2147483647, "typed depth forwards signed value to any installed handler");
	set_profiling(false);
	draw_group(handler);
	check(handler.calls == 7 && get_profile_records().size() == initial_records, "disabled render profiling");
	set_profiling(true);
	draw_group(handler);
	const char* names[] = { "beginDisplay", "endDisplay", "drawMeshStrip", "drawLineStrip", "drawBitmap", "setText", "drawText" };
	for (const char* name : names)
	{
		profile_record* counter = record(name);
		check(counter && counter->m_count == 1, name);
	}
	draw_group(handler);
	for (const char* name : names) check(record(name)->m_count == 2, "record reused");
	set_profiling(false);
	draw_group(handler);
	for (const char* name : names) check(record(name)->m_count == 2, "disabled does not count draws");
	set_render_handler(NULL);
	render::set_depth(123);
	check(handler.depth == -2147483647, "absent depth handler has no effect");
	bitmap_ownership_checks(handler);
	bitmap_export_checks(handler);

	as_environment env;
	env.m_local_frames.push_back(as_environment::frame_slot("x", as_value(1)));
	env.m_local_frames.push_back(as_environment::frame_slot());
	env.m_local_frames.push_back(as_environment::frame_slot("x", as_value(2)));
	check(env.find_local("x") == 2, "latest local");
	profile_record* lookup = record("find_local");
	check(lookup && lookup->m_count == 1, "lookup counts with profiling disabled");
	check(env.find_local("missing") == -1 && lookup->m_count == 2, "lookup miss counts");
	set_profiling(true);
	env.m_local_frames.pop_back();
	check(env.find_local("x") == -1 && lookup->m_count == 3, "frame barrier counts");
	env.m_local_frames.pop_back();
	check(env.find_local("x") == 0 && lookup->m_count == 4, "lookup hit counts");
	set_profiling(false);
	native_key_checks();
	s4_removal_checks();

	rect zero = { -0.0f, -0.0f, -0.0f, -0.0f };
	zero.expand_to_point(0.0f, 0.0f);
	check(!std::signbit(zero.m_x_min) && !std::signbit(zero.m_y_min), "minimum selects input zero");
	check(std::signbit(zero.m_x_max) && std::signbit(zero.m_y_max), "maximum retains old zero");
	const float nan = std::numeric_limits<float>::quiet_NaN();
	rect unordered = { 1, 3, 2, 4 };
	unordered.expand_to_point(nan, nan);
	check(std::isnan(unordered.m_x_min) && std::isnan(unordered.m_y_min), "minimum selects input NaN");
	check(unordered.m_x_max == 3 && unordered.m_y_max == 4, "maximum rejects input NaN");
	unordered = { nan, nan, nan, nan };
	unordered.expand_to_point(7, 8);
	check(unordered.m_x_min == 7 && unordered.m_y_min == 8 && std::isnan(unordered.m_x_max) && std::isnan(unordered.m_y_max), "stored NaN asymmetry");
	rect a = { -2, 6, 4, 10 };
	rect b = { -8, 3, -4, 12 };
	a.expand_to_rect(b);
	check(a.m_x_min == -8 && a.m_x_max == 6 && a.m_y_min == -4 && a.m_y_max == 12, "rectangle union");
	matrix flip;
	flip.m_[0][0] = -2; flip.m_[0][2] = 3; flip.m_[1][2] = -5;
	rect diagonal = { 1, 4, 2, 6 };
	diagonal.transform(flip);
	check(diagonal.m_x_min == 1 && diagonal.m_x_max == -5 && diagonal.m_y_min == -3 && diagonal.m_y_max == 1, "two-corner transform keeps inversion");
	rect enclosed;
	const rect original = { 1, 4, 2, 6 };
	enclosed.enclose_transformed_rect(flip, original);
	check(enclosed.m_x_min == -5 && enclosed.m_x_max == 1 && enclosed.m_y_min == -3 && enclosed.m_y_max == 1, "enclosure sorts transformed corners");
	a = { 0, 2, 4, 6 }; b = { 2, 6, 10, 14 };
	a.set_lerp(a, b, 0.5f);
	check(a.m_x_min == 1 && a.m_x_max == 4 && a.m_y_min == 7 && a.m_y_max == 10, "aliased interpolation");
	shape_character_def shape;
	shape.set_bound(rect{ 1, 4, 2, 6 });
	check(shape.point_test_local(2, 3), "shape hit uses rectangle without paths");
	check(!shape.point_test_local(0, 3), "shape outside rectangle");
	set_render_handler(&handler);
	gradient_checks(handler);
	set_render_handler(NULL);
	timer_checks();
	bounds_checks();
	focus_lifetime_checks();
	accessor_lifetime_checks();
	graph_checks();
	text_graph_checks();
	text_property_checks();
	matrix_interface_checks();
	init_action_interface_checks();
	character_interface_checks();
	vm_interface_checks();
	container_interface_checks();
	log_buffer_checks();
	std::printf("swf_iface_runtime: %d checks passed\n", checks);
}
