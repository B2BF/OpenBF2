// [TEMP] [CHOSEN, not recovered] Content-free observer for the native runtime
// image rectangle. Uses production constructors and tessellation; no GPU or game.
#include "gameswf/gameswf_shape.h"
#include "gameswf/gameswf_tesselate.h"
#include "gameswf/gameswf_render.h"
#include "base/tu_file.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <type_traits>

using namespace gameswf;

static void check(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

struct observer : tesselate::trapezoid_accepter
{
    unsigned fills = 0;
    double area = 0;
    void accept_trapezoid(int style, const tesselate::trapezoid& value) override
    {
        check(style == 0, "rectangle preserves its zero-based fill");
        ++fills;
        area += double(value.m_y1 - value.m_y0) *
            double(value.m_rx0 - value.m_lx0 + value.m_rx1 - value.m_lx1) / 2;
    }
    void accept_line_strip(int, const point*, int) override
    {
        check(false, "filled rectangle has no line style");
    }
};

static unsigned char constructor_flag(unsigned char initial)
{
    std::aligned_storage<sizeof(path), alignof(path)>::type storage;
    std::memset(&storage, initial, sizeof(storage));
    path* value = new (&storage) path(0, 0, 1, 0, 0);
    // Inspect the object representation only: reading the uninitialized bool as
    // a bool would itself be undefined. The tessellation cases below initialize it.
    unsigned char flag = 0;
    std::memcpy(&flag, &value->m_new_shape, sizeof(flag));
    value->~path();
    return flag;
}

static observer rectangle(bool new_shape)
{
    shape_character_def shape;
    path marker;
    marker.m_new_shape = true;
    shape.append_path(marker);
    path quad(0, 0, 1, 0, 0);
    quad.m_new_shape = new_shape;
    quad.m_edges.push_back(edge(0, 0, 5120, 0));
    quad.m_edges.push_back(edge(5120, 5120, 5120, 5120));
    quad.m_edges.push_back(edge(0, 5120, 0, 5120));
    quad.m_edges.push_back(edge(0, 0, 0, 0));
    shape.append_path(quad);
    observer result;
    shape.tesselate(1, &result);
    return result;
}

// Only the image-descriptor boundary is synthetic. The movie, ActionScript
// loadMovie callback, display-list ownership and shape tessellation are real.
struct image_source : gameswf::render_handler
{
    unsigned loads = 0;
    bitmap_info* create_bitmap_info_file(const char* name) override
    {
        check(std::strcmp(name, "synthetic-image") == 0, "image URL reaches renderer");
        ++loads;
        bitmap_info* result = new bitmap_info;
        result->m_texture_id = 42;
        result->m_original_width = 256;
        result->m_original_height = 256;
        return result;
    }
    void delete_bitmap_info(bitmap_info*) override {}
    bitmap_info* create_bitmap_info_empty() override { std::abort(); }
    bitmap_info* create_bitmap_info_alpha(int, int, unsigned char*) override { std::abort(); }
    bitmap_info* create_bitmap_info_rgb(image::rgb*) override { std::abort(); }
    bitmap_info* create_bitmap_info_rgba(image::rgba*) override { std::abort(); }
    void set_depth(int) override { std::abort(); }
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
    void draw_bitmap(const matrix&, const bitmap_info*, const rect&, const rect&, rgba) override { std::abort(); }
    void text_set(void*, const wchar_t*) override { std::abort(); }
    void text_draw(void*, const matrix*, unsigned, int) override { std::abort(); }
    void set_antialiased(bool) override { std::abort(); }
    void begin_submit_mask() override { std::abort(); }
    void end_submit_mask() override { std::abort(); }
    void disable_mask() override { std::abort(); }
};

static void runtime_image()
{
    image_source source;
    set_render_handler(&source);
    {
        // One-pixel, one-frame synthetic FWS containing no script or assets.
        unsigned char bytes[] = { 0x46, 0x57, 0x53, 0x07, 0x14, 0, 0, 0,
            0x30, 0x0a, 0, 0xa0, 0, 0x0c, 1, 0, 0x40, 0, 0, 0 };
        tu_file input(tu_file::memory_buffer, sizeof(bytes), bytes);
        smart_ptr<movie_definition> definition = create_movie_no_recurse(&input,
            DO_LOAD_BITMAPS, DO_NOT_LOAD_FONT_SHAPES);
        check(definition != NULL, "synthetic movie parses");
        smart_ptr<movie_interface> instance = definition->create_instance();
        movie* root = instance->get_root_movie();
        as_environment* environment = root->get_action_environment();
        as_value method;
        check(root->get_member("loadMovie", &method), "loadMovie is registered");
        as_c_function_ptr load = method.to_c_function();
        check(load != NULL, "loadMovie has a native callback");
        environment->push("synthetic-image");
        as_value result;
        load(fn_call(&result, root, environment, 1, environment->get_top_index()));
        environment->drop(1);
        check(source.loads == 1, "one image descriptor is loaded");
        as_value image_sprite, image_shape;
        check(root->get_member("_____image", &image_sprite), "image sprite is published");
        check(image_sprite.to_object()->get_member("_____image_shape", &image_shape),
            "image shape is published");
        generic_character* placed = static_cast<generic_character*>(image_shape.to_object()->cast_to_character());
        shape_character_def* shape = static_cast<shape_character_def*>(placed->m_def.get_ptr());
        check(shape->get_paths().size() == 2, "image has marker and rectangle paths");
        unsigned char flag = 0xff;
        std::memcpy(&flag, &shape->get_paths()[1].m_new_shape, sizeof(flag));
        check(flag == 0, "native loadMovie initializes its rectangle flag");
        observer geometry;
        shape->tesselate(1, &geometry);
        check(geometry.fills != 0 && geometry.area == 5120.0 * 5120.0,
            "published image rectangle produces its complete filled area");
    }
    set_render_handler(NULL);
}

int main()
{
    check(constructor_flag(0) == 0, "parameterized constructor retains zero storage");
    check(constructor_flag(1) == 1, "parameterized constructor retains marker storage");
    const observer skipped = rectangle(true);
    check(skipped.fills == 0, "new-shape marker skips all rectangle edges");
    const observer drawn = rectangle(false);
    check(drawn.fills != 0 && drawn.area == 5120.0 * 5120.0,
        "initialized rectangle tessellates its complete area");
    runtime_image();
    std::printf("PASS: poisoned constructor flag retained; marker emits %u fills; rectangle emits %u fills, area %.0f\n",
        skipped.fills, drawn.fills, drawn.area);
}
