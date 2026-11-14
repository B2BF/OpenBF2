// [TEMP] [CHOSEN, not recovered] Observe the production font rasterizer directly.
// The included TU exposes its private software buffer to this synthetic fixture.
// No engine, renderer device, installed content or network is used.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#ifndef BF2_SWF_FONTLIB_SOURCE
#define BF2_SWF_FONTLIB_SOURCE "../BF2/External/gameswf/gameswf_fontlib.cpp"
#endif
#include BF2_SWF_FONTLIB_SOURCE

using namespace gameswf;
using namespace gameswf::fontlib;
static int checks;
static void check(bool condition, const char* message)
{
	++checks;
	if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}

static void scanline_checks()
{
	Uint8 pixels[64] = {};
	s_render_buffer = pixels;
	s_glyph_render_size = 8;
	software_trapezoid(0, 4, 1, std::nextafter(1.0f, 2.0f), 5, 5);
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 8; ++x)
			check(pixels[y * 8 + x] == ((y < 4 && x >= (y ? 2 : 1) && x < 5) ? 255 : 0),
				"interpolation retains fractional edge through double ceil");
	std::memset(pixels, 0, sizeof(pixels));
	software_trapezoid(-2, 10, -4, -4, 20, 20);
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 8; ++x)
			check(pixels[y * 8 + x] == (x < 7 ? 255 : 0), "scanline clipping excludes final column");
	std::memset(pixels, 0, sizeof(pixels));
	software_trapezoid(0, 1, 5, 5, 2, 2);
	software_trapezoid(1, 1, 0, 0, 5, 5);
	software_trapezoid(std::numeric_limits<float>::quiet_NaN(), -2147483648.0f, 0, 0, 5, 5);
	for (int i = 0; i < 64; ++i) check(pixels[i] == 0, "empty/reversed/nonfinite raster endpoints");
	software_trapezoid(0, 1, std::numeric_limits<float>::infinity(), 0, 5, 5);
	for (int i = 0; i < 64; ++i) check(pixels[i] == (i < 5 ? 255 : 0),
		"unordered edge truncates to integer indefinite before clipping");
	std::memset(pixels, 0, sizeof(pixels));
	s_render_matrix.set_identity();
	s_render_matrix.m_[0][0] = std::nextafter(1.0f, 2.0f);
	s_render_matrix.m_[0][2] = -(1.0f + std::ldexp(1.0f, -22));
	const tesselate::trapezoid trapezoid = { 0, 1, std::nextafter(1.0f, 2.0f),
		std::nextafter(1.0f, 2.0f), 4, 4 };
	draw_into_software_buffer accepter;
	accepter.accept_trapezoid(99, trapezoid);
	for (int y = 0; y < 8; ++y)
		for (int x = 0; x < 8; ++x)
			check(pixels[y * 8 + x] == ((y == 0 && x >= 1 && x < 4) ? 255 : 0),
				"transformed edge preserves product cancellation before float store");
	s_render_buffer = NULL;
}

struct outline : shape_character_def
{
	mutable float tolerance = 0;
	mutable int calls = 0;
	outline()
	{
		path p(-100, -100, 1, 0, 0);
		p.m_edges.push_back(edge(100, 100, 100, 100));
		append_path(p);
	}
	void tesselate(float error, tesselate::trapezoid_accepter* accepter) const override
	{
		tolerance = error; ++calls;
		// Native callable observer supplies a rectangle in the outline's coordinates.
		const tesselate::trapezoid rectangle = { -100, 100, -100, -100, 100, 100 };
		accepter->accept_trapezoid(1, rectangle);
	}
};

static void outline_checks()
{
	s_glyph_nominal_size = 8;
	s_glyph_render_size = 32;
	s_rendering_box = 1536;
	Uint8 pixels[1024];
	s_render_buffer = pixels;
	outline shape;
	rendered_glyph_info result;
	check(render_glyph(&result, &shape), "nonempty outline produces image");
	check(shape.calls == 1 && shape.tolerance == 24, "outline dispatch and tolerance");
	check(result.m_image->m_width == 2 && result.m_image->m_height == 1, "oversample bounds crop");
	// Rounded transform translation leaves the left edge just above zero.
	check(image::scanline(result.m_image, 0)[0] == 191
		&& image::scanline(result.m_image, 0)[1] == 63, "four-by-four sample coverage");
	check(result.m_offset_x == float(100.0L / 192.0L)
		&& result.m_offset_y == float(1436.0L / 192.0L - 7), "image origin uses shared wide ratio");
	check(result.m_image_hash == result.m_image->compute_hash(), "image hash published after copying");
	delete result.m_image;
	s_render_buffer = NULL;
}

struct bitmap_observer : gameswf::render_handler
{
	rect coordinates;
	int calls = 0;
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
	void fill_style_disable(int) override { std::abort(); }
	void fill_style_color(int, rgba) override { std::abort(); }
	void fill_style_bitmap(int, const bitmap_info*, const matrix&, bitmap_wrap_mode) override { std::abort(); }
	void line_style_disable() override { std::abort(); }
	void line_style_color(rgba) override { std::abort(); }
	void line_style_width(float) override { std::abort(); }
	void draw_bitmap(const matrix&, const bitmap_info* bitmap, const rect& bounds,
		const rect& uv, rgba color) override
	{
		check(bitmap != NULL && uv.m_x_min == 16777216.0f && color.m_r == 7, "bitmap forwarding");
		coordinates = bounds; ++calls;
	}
	void set_antialiased(bool) override { std::abort(); }
	void begin_submit_mask() override { std::abort(); }
	void end_submit_mask() override { std::abort(); }
	void disable_mask() override { std::abort(); }
};

static void bitmap_checks()
{
	bitmap_observer observer;
	set_render_handler(&observer);
	texture_glyph glyph;
	glyph.m_bitmap_info = new bitmap_info;
	glyph.m_uv_bounds = rect{ 16777216, 16777220, 16777216, 16777220 };
	glyph.m_uv_origin = point(-1, -1);
	matrix transform;
	draw_glyph(transform, glyph, rgba(7, 8, 9, 10), 131072);
	check(observer.calls == 1 && observer.coordinates.m_x_min == 50331652.0f
		&& observer.coordinates.m_y_min == 50331648.0f, "X subtraction stays wide while Y narrows");
	const rect first = observer.coordinates;
	draw_glyph(transform, glyph, rgba(7, 8, 9, 10), 65536);
	check(observer.calls == 2 && observer.coordinates.m_x_min == first.m_x_min,
		"first nominal height fixes the cached scale");
	set_render_handler(NULL);
}

static char clamp_message[200];
static void observe_clamp(bool error, const char* message)
{
	check(error, "glyph clamp reports an error");
	std::snprintf(clamp_message, sizeof(clamp_message), "%s", message);
}

static void nominal_size_checks()
{
	register_log_callback(NULL);
	for (int value : { -1, 0, 3, 4, 64, 127, 128 })
	{
		set_nominal_glyph_pixel_size(value);
		const int expected = value < 4 ? 4 : value;
		check(s_glyph_nominal_size == expected && s_glyph_render_size == expected * 4,
			"nominal glyph clamp and fourfold render size");
	}
	register_log_callback(observe_clamp);
	set_nominal_glyph_pixel_size(129);
	check(s_glyph_nominal_size == 128 && s_glyph_render_size == 512,
		"upper nominal glyph clamp");
	check(std::strcmp(clamp_message, "set_nominal_glyph_pixel_size(129) too large, clamping to 128\n") == 0,
		"upper clamp formats both supplied arguments");
	register_log_callback(NULL);
	set_nominal_glyph_pixel_size(96);
}

int main(int argc, char** argv)
{
	if (argc == 1 || std::strcmp(argv[1], "--nominal-size") == 0) nominal_size_checks();
	if (argc == 1 || std::strcmp(argv[1], "--scanline") == 0) scanline_checks();
	if (argc == 1 || std::strcmp(argv[1], "--outline") == 0) outline_checks();
	if (argc == 1 || std::strcmp(argv[1], "--bitmap") == 0) bitmap_checks();
	std::printf("swf_font_raster_runtime: %d checks passed\n", checks);
}
