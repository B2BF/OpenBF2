// gameswf.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// SWF (Shockwave Flash) player library.  The info for this came from
// http://www.openswf.org, the flashsource project, and swfparse.cpp


#ifndef GAMESWF_H
#define GAMESWF_H


#include <cstdint>
#include <memory>
#include <string>
#include <ctype.h>	// for poxy wchar_t
#include <stdarg.h>

class tu_file;
class render_handler;
class weak_proxy;	// forward decl; defined in base/smart_ptr.h

// @@ forward decl to avoid including base/image.h; TODO change the
// render_handler interface to not depend on these structs at all.
namespace image { struct rgb; struct rgba; }

// forward decl
namespace jpeg { struct input; }
class tu_string;
class tu_stringi;


namespace gameswf
{
	// Forward declarations.
	struct action_buffer;
	struct as_environment;
	struct as_value;
	struct bitmap_info;
	struct character;
	struct execute_tag;
	struct font;
	struct movie;
	struct movie_interface;
	struct render_handler;
	struct resource;
	struct rgba;
	struct sound_handler;
	struct stream;
	
	//
	// Log & error reporting control.
	//
	
	// Supply a function pointer to receive log & error messages.
	void	register_log_callback(void (*callback)(bool error, const char* message));
	
	// Control verbosity of specific categories.
	void	set_verbose_action(bool verbose);
	void	set_verbose_parse(bool verbose);
	void	set_verbose_debug(bool verbose);
	// [INFERRED] names of the separate retail loader and live-count log switches.
	void	set_verbose_loader(bool verbose);
	void	set_verbose_gc(bool verbose);
	
	// Get and set the render handler.  This is one of the first
	// things you should do to initialise the player (assuming you
	// want to display anything).
	void    set_render_handler(render_handler* s);

	// Pass in a sound handler, so you can handle audio on behalf of
	// gameswf.  This is optional; if you don't set a handler, or set
	// NULL, then sounds won't be played.
	//
	// If you want sound support, you should set this at startup,
	// before loading or playing any movies!
	void	set_sound_handler(sound_handler* s);

	// You probably don't need this. (@@ make it private?)
	sound_handler*	get_sound_handler();

	// Register a callback to the host, for providing a file,
	// given a "URL" (i.e. a path name).  This is the only means
	// by which the gameswf library accesses file data, for
	// loading movies, cache files, and so on.
	//
	// gameswf will call this when it needs to open a file.
	//
	// NOTE: the returned tu_file* will be delete'd by gameswf
	// when it is done using it.  Your file_opener_function may
	// return NULL in case the requested file can't be opened.
	typedef tu_file* (*file_opener_callback)(const char* url_or_path);
	void	register_file_opener_callback(file_opener_callback opener);

	// ActionScripts embedded in a movie can use the built-in
	// fscommand() function to send data back to the host
	// application.  If you are interested in this data, register
	// a handler, which will be called when the embedded scripts
	// call fscommand().
	//
	// The handler gets the movie_interface* that the script is
	// embedded in, and the two string arguments passed by the
	// script to fscommand().
	typedef void (*fscommand_callback)(movie_interface* movie, const char* command, const char* arg);
	void	register_fscommand_callback(fscommand_callback handler);

	// Use this to control how finely curves are subdivided.  1.0
	// is the default; it's a pretty good value.  Larger values
	// result in coarser, more angular curves with fewer vertices.
	void	set_curve_max_pixel_error(float pixel_error);
	float	get_curve_max_pixel_error();
	
	// For stuff that's tricky to keep track of w/r/t ownership & cleanup.
	struct ref_counted
	{
		ref_counted();
		// SwiffPlayer.dll 0x100A4850 / 0x100A0180: implicit copy construction
		// and assignment copy count and proxy without retaining the proxy.
		ref_counted(const ref_counted&) = default;
		ref_counted& operator=(const ref_counted&) = default;
		virtual ~ref_counted();
		// [CHOSEN, not recovered] issue #7: retail script interfaces carry a
		// "reference counted" query in slot 0 and add_ref in slot +4 (characters:
		// 0x1007EDA0 true / 0x1007EFE0; intermediate stage 0x1007DF50 false; plain
		// objects 0x10008EE0 false), tested before each value-construction add_ref
		// (e.g. 0x10091048-0x10091055). Native objects are always reference counted.
		void	add_ref() const;
		void	drop_ref() const;
		int	get_ref_count() const { return m_ref_count; }
		weak_proxy*	get_weak_proxy() const;

	private:
		mutable int	m_ref_count;
		mutable weak_proxy*	m_weak_proxy;
	};


	struct font;
	struct character_def;
	struct bitmap_character_def;
	struct rect;
	struct sound_sample;
	
	// An interface for casting to different types of
	// resources.
	// SwiffPlayer.dll: ref_counted is a VIRTUAL base, shared by the resource/character_def and the
	// movie_definition sides (sprite_definition ctor 0x1007B5A0 keeps one copy at +0x40 behind the
	// vbtables 0x100E01D4 / 0x100E01CC; movie_def_impl ctor 0x10077FB0 at +0x90 behind 0x100DFD24).
	struct resource : virtual public ref_counted
	{
		// SwiffPlayer.dll 0x10071460 / 0x10073020: only the reference-counted
		// base has state; 0x10072FF0 is the compiler's deleting wrapper.
		resource() = default;
		virtual ~resource() = default;

		// Override in derived classes that implement corresponding interfaces.
		// SwiffPlayer.dll 0x1006EC20 / 0x1006EC30 / 0x1006EC40
		// (resource table 0x100DF258); [INFERRED] source-level names.
		virtual font*	cast_to_font() { return 0; }
		virtual character_def*	cast_to_character_def() { return 0; }
		virtual sound_sample*	cast_to_sound_sample() { return 0; }
	};


	struct as_object;
	struct as_object_interface;

	// The registry of live script objects, kept by as_object_interface's constructors and
	// destructor so the movie teardown (release_script_objects) also reaches objects that only
	// reference each other. [CHOSEN, not recovered] issue #7
	// (https://github.com/B2BF/OpenBF2/issues/7).
	void	register_script_object(as_object_interface* obj);
	void	unregister_script_object(as_object_interface* obj);
	// Shared generation read by the retail script graph's mark visitors.
	std::uint32_t get_gc_mark_generation();
	std::uint32_t get_gc_release_generation();

	// This is the base class for all ActionScript-able objects
	// ("as_" stands for ActionScript).
	struct as_object_interface : public resource
	{
		// SwiffPlayer.dll 0x1007ECC2 / 0x1009A1C7 initializes the common
		// list links to null. Collection owns insertion; construction does not.
		as_object_interface* m_gc_previous = NULL;
		as_object_interface* m_gc_next = NULL;
		// [CHOSEN, not recovered] S3 bridge: native state for retail's secondary
		// interface generation field; this does not reproduce its x86 base layout.
		std::uint32_t m_gc_generation = 0;

		// [CHOSEN, not recovered] issue #7: registration, see register_script_object.
		as_object_interface() { register_script_object(this); }
		as_object_interface(const as_object_interface& other) : resource(other) { register_script_object(this); }
		virtual ~as_object_interface() { unregister_script_object(this); }

		// [CHOSEN, not recovered] issue #7: a safe downcast for the movie teardown; character
		// answers itself. movie declares the same slot (gameswf_impl.h).
		virtual character*	cast_to_character() { return 0; }

		// [CHOSEN, not recovered] Native type adjustment (issue #7), following the cast_to_font /
		// cast_to_character_def pattern above. Needed by the AS2 `addProperty` builtin, whose
		// fn.this_ptr is an as_object_interface* that may in fact be a sprite_instance - a
		// static_cast would silently misinterpret those. See GAMESWF_DIVERGENCES.md.
		virtual as_object*	cast_to_as_object() { return 0; }

		// So that text_character's can return something reasonable.
		// SwiffPlayer.dll 0x1007DF10, movie secondary tables +0x18.
		virtual const char*	get_text_value() const { return 0; }

		// [CHOSEN, not recovered] Native accessor for the recovered wide payload (the shipped
		// tu_string_as_object carries a std::wstring at +60 beside the narrow tu_string at +44). The
		// text field's `text` setter (sub_10089440) routes a value carrying one to its wide store;
		// every other object answers NULL. See GAMESWF_DIVERGENCES.md.
		virtual const unsigned short*	get_wide_text_value() const { return 0; }   // (Uint16; the base typedef is not visible here)

		virtual void	set_member(const tu_stringi& name, const as_value& val) = 0;
		virtual bool	get_member(const tu_stringi& name, as_value* val) = 0;
		virtual movie*	to_movie() = 0;

		// SwiffPlayer.dll 0x1007DB80 / 0x1007DB70: leaf visitors only update
		// the shared generation, and do nothing when it already matches.
		virtual void gc_mark()
		{
			const std::uint32_t generation = get_gc_mark_generation();
			if (m_gc_generation != generation) m_gc_generation = generation;
		}
		virtual void gc_release()
		{
			const std::uint32_t generation = get_gc_release_generation();
			if (m_gc_generation != generation) m_gc_generation = generation;
		}

		// SwiffPlayer.dll 0x10008F50, movie tables 0x100E08B4/0x100E0C3C
		// +0x28: movies contribute no enumeration names.
		virtual void enumerate(as_environment* env) {}
	};


	// For caching precomputed stuff.  Generally of
	// interest to gameswf_processor and programs like it.
	struct cache_options
	{
		bool	m_include_font_bitmaps;
		
		cache_options()
			:
			m_include_font_bitmaps(true)
		{
		}
	};


	// A character_def is the immutable data representing the template of a
	// movie element.
	//
	// @@ This is not really a public interface.  It's here so it
	// can be mixed into movie_definition, movie_definition_sub,
	// and sprite_definition, without using multiple inheritance.
	struct character_def : public resource
	{
	private:
		int	m_id;
		
	public:
		character_def()
			:
			m_id(-1),
			m_registered_class(0)	//
		{
		}

		// SwiffPlayer.dll 0x1006EDB0 reads the definition ID, as does 0x100759C9.
		// [CHOSEN, not recovered] S3 bridge: the native loader assigns it separately
		// from an instance's placement ID. Root and runtime definitions use zero.
		int get_character_id() const { return m_id; }
		void set_character_id(int id) { m_id = id; }

		// Compiler deleting wrapper: SwiffPlayer.dll 0x100730C0. This forwards
		// to the destructor before conditional deletion; its base lifetime is separate.
		virtual ~character_def()
		{
			// [CHOSEN, not recovered] S3 bridge: native definitions own registered
			// classes. Retail 0x1006ED80/0x1006EF50 only change base vtables here.
			if (m_registered_class) m_registered_class->drop_ref();
		}

		// SwiffPlayer.dll 0x1006EDD0; definition tables +0x10.
		virtual void	display(character* instance_info) {}
		// SwiffPlayer.dll 0x1006EDE0 (release default).
		virtual bool	point_test_local(float x, float y) { return false; }
		// SwiffPlayer.dll 0x1006EDF0 / 0x1006EE00 return positive zero.
		virtual float	get_height_local() { return 0.0f; }
		virtual float	get_width_local() { return 0.0f; }
		// SwiffPlayer.dll character_def table 0x100DF268 +0x20 is _purecall: every concrete definition
		// answers its rectangle (shape and morph 0x10099290, text 0x100853E0, edit text 0x10086B40,
		// bitmap 0x100739E0, sprite 0x1007B670, button 0x100A1F90). Callers dispatch through it
		// directly (generic character 0x1007F240 via its definition, button bounds 0x100A1F90).
		virtual rect	get_bound_copy() const = 0;

		// Should stick the result in a smart_ptr immediately.
		virtual character*	create_character_instance(movie* parent, int id);	// default is to make a generic_character

		// From resource interface.
		// SwiffPlayer.dll 0x1006EE10 returns its receiver.
		virtual character_def*	cast_to_character_def() { return this; }

		// [CHOSEN, not recovered] Native bitmap type adjustment, same pattern as the casts
		// above. Used by movie_definition::get_exported_bitmaps to tell an exported bitmap from any
		// other exported character. Overridden in bitmap_character_def. See GAMESWF_DIVERGENCES.md.
		virtual bitmap_character_def*	cast_to_bitmap_character_def() { return 0; }

		// the AS2 class bound to this definition by Object.registerClass
		// (symbol, Class). The shipped character_def carries it at +12 (sub_100997B0 stores the
		// function there after get_exported_resource(symbol)); a sprite instanced from such a
		// definition gets __proto__ = Class.prototype and its constructor run at load
		// (sub_10081870). A raw ref_counted pointer, as shipped, because as_as_function is opaque
		// to this header. Native registerClass and teardown retain/release it through
		// the existing S3 ownership bridge. Stock 2006 has no registerClass.
		ref_counted*	m_registered_class;

		//
		// Caching.
		//

		// SwiffPlayer.dll 0x1006EE20 / 0x1006EE30: release-image defaults.
		virtual void	output_cached_data(tu_file* out, const cache_options& options) {}
		virtual void	input_cached_data(tu_file* in) {}
	};


	//
	// This is the client program's interface to the definition of
	// a movie (i.e. the shared constant source info).
	//
	// SwiffPlayer.dll: a movie definition is NOT a character definition. movie_definition derives only
	// from the shared virtual ref_counted (movie_def_impl ctor 0x10077FB0: primary table 0x100DFC90,
	// whose +0x20 is visit_imported_movies, not the bounds slot). Sprite definitions add character_def
	// as a second base.
	struct movie_definition : virtual public ref_counted
	{
		// Compiler deleting wrapper: SwiffPlayer.dll 0x10071680. Native bases
		// retain their ownership adapters; this certifies the wrapper only.
		~movie_definition() override = default;
		virtual int	get_version() const = 0;
		virtual float	get_width_pixels() const = 0;
		virtual float	get_height_pixels() const = 0;
		virtual int	get_frame_count() const = 0;
		virtual float	get_frame_rate() const = 0;

		// This calls add_ref() on the movie_interface internally.
		// Call drop_ref() on the movie_interface when you're done with it.
		// Or use smart_ptr<T> from base/smart_ptr.h if you want.
		virtual movie_interface*	create_instance() = 0;

		virtual void	output_cached_data(tu_file* out, const cache_options& options) = 0;
		virtual void	input_cached_data(tu_file* in) = 0;

		// Causes this movie def to generate texture-mapped
		// versions of all the fonts it owns.  This improves
		// speed and quality of text rendering.  The
		// texture-map data is serialized in the
		// output/input_cached_data() calls, so you can
		// preprocess this if you load cached data.
		virtual void	generate_font_bitmaps() = 0;

		//
		// (optional) API to support gameswf::create_movie_no_recurse().
		//

		// Call visit_imported_movies() to retrieve a list of
		// names of movies imported into this movie.
		// visitor->visit() will be called back with the name
		// of each imported movie.
		struct import_visitor
		{
			virtual void	visit(const char* imported_movie_filename) = 0;
		};
		virtual void	visit_imported_movies(import_visitor* visitor) = 0;

		// Call this to resolve an import of the given movie.
		// Replaces the dummy placeholder with the real
		// movie_definition* given.
		virtual void	resolve_import(const char* name, movie_definition* def) = 0;

		//
		// (optional) API to support host-driven creation of textures.
		//
		// Create the movie using gameswf::create_movie_no_recurse(..., DO_NOT_LOAD_BITMAPS),
		// and then initialize each bitmap info via get_bitmap_info_count(), get_bitmap_info(),
		// and bitmap_info::init_*_image() or your own subclassed API.
		//
		// E.g.:
		//
		// // During preprocessing:
		// // This will create bitmap_info's using the rgba, rgb, alpha contructors.
		// my_def = gameswf::create_movie_no_recurse("myfile.swf", DO_LOAD_BITMAPS);
		// int ct = my_def->get_bitmap_info_count();
		// for (int i = 0; i < ct; i++)
		// {
		//	my_bitmap_info_subclass*	bi = NULL;
		//	my_def->get_bitmap_info(i, (bitmap_info**) &bi);
		//	my_precomputed_textures.push_back(bi->m_my_internal_texture_reference);
		// }
		// // Save out my internal data.
		// my_precomputed_textures->write_into_some_cache_stream(...);
		//
		// // Later, during run-time loading:
		// my_precomputed_textures->read_from_some_cache_stream(...);
		// // This will create blank bitmap_info's.
		// my_def = gameswf::create_movie_no_recurse("myfile.swf", DO_NOT_LOAD_BITMAPS);
		// 
		// // Push cached texture info into the movie's bitmap_info structs.
		// int	ct = my_def->get_bitmap_info_count();
		// for (int i = 0; i < ct; i++)
		// {
		//	my_bitmap_info_subclass*	bi = (my_bitmap_info_subclass*) my_def->get_bitmap_info(i);
		//	bi->set_internal_texture_reference(my_precomputed_textures[i]);
		// }
		// enumerate the bitmaps that ExportAssets gave a NAME.
		//
		// This is the missing half of the documented host-driven texture API above. BF2's menu SWFs
		// carry each menu bitmap as an undecoded DefineBitsLossless character whose ExportAssets
		// symbol IS ITS EXTERNAL FILE PATH (e.g. id=34 -> "images/UIcomponents/button-bkg-7.png";
		// 12/12 of mainMenu.swf's image-named exports are DefineBits characters). The host resolves
		// each by name through ISwiffRender::loadTexture and writes the handle into the bitmap_info,
		// which is why the shipped DLL links neither zlib nor jpeglib - it never decodes a pixel.
		// gameswf keeps name->resource in m_exports but exposes no way to walk it; this does.
		// Fills up to `max` pairs, returns the count written. The names are COPIED into the caller's
		// tu_string array: the export hash's iterator hands its key back BY VALUE (container.h
		// get_key()), so a `const char*` taken from it dangled and read back as "" or garbage once
		// the heap block was reused - which is exactly what retail showed (2026-09-02: 1 of 12
		// exports resolved, names empty). See GAMESWF_DIVERGENCES.md.
		virtual int	get_exported_bitmaps(int max, tu_string* names,
						     bitmap_info** infos) const { return 0; }

		virtual int	get_bitmap_info_count() const = 0;
		virtual bitmap_info*	get_bitmap_info(int i) const = 0;
	};


	//
	// This is the client program's interface to an instance of a
	// movie (i.e. an independent stateful live movie).
	//
	struct movie_interface : public as_object_interface
	{
		// Compiler deleting wrapper: SwiffPlayer.dll 0x10073100; base lifetime is separate.
		~movie_interface() override = default;
		virtual movie_definition*	get_movie_definition() = 0;

		// Frame counts in this API are 0-based (unlike ActionScript)
		virtual int	get_current_frame() const = 0;
		virtual bool	has_looped() const = 0;
		
		virtual void	restart() = 0;
		virtual void	advance(float delta_time) = 0;
		virtual void	goto_frame(int frame_number) = 0;
		// Returns true if labeled frame is found.
		virtual bool	goto_labeled_frame(const char* label) = 0;
		virtual void	display() = 0;

		enum play_state
		{
			PLAY,
			STOP
		};
		virtual void	set_play_state(play_state s) = 0;
		virtual play_state	get_play_state() const = 0;
		
		virtual void	set_background_color(const rgba& bg_color) = 0;
		// the getter the shipped SwiffPlayer::update reads every frame -
		// movie_interface vslot +48 in SwiffPlayer.dll (2).i64 is sub_100736C0, `return this+32`, the
		// address of movie_root::m_background_color - and pushes to ISwiffRender::setBackgroundColor
		// (verified on retail 2026-09-02: 0xFF666666 after every updateTime). Stock 2006 has only the
		// setter. movie_root answers with its m_background_color; movie (gameswf_impl.h) carries the
		// no-background default, next to its set_background_color stub. See GAMESWF_DIVERGENCES.md.
		virtual rgba	get_background_color() const = 0;

		// Set to 0 if you don't want the movie to render its
		// background at all.  1 == full opacity.
		virtual void	set_background_alpha(float alpha) = 0;
		virtual float	get_background_alpha() const = 0;
		
		// move/scale the movie...
		virtual void	set_display_viewport(int x0, int y0, int w, int h) = 0;
		
		// Input.
		virtual void	notify_mouse_state(int x, int y, int buttons) = 0;
		
		// Set an ActionScript variable within this movie.
		// You can use this to set the value of text fields,
		// ordinary variables, or properties of characters
		// within the script.
		//
		// This version accepts UTF-8
		virtual void	set_variable(const char* path_to_var, const char* new_value) = 0;
		// This version accepts UCS-2 or UCS-4, depending on sizeof(wchar_t)
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value) = 0;
		// @@ do we want versions that take a number?

		// Get the value of an ActionScript variable.
		//
		// Value is ephemeral & not thread safe!!!  Use it or
		// copy it immediately.
		//
		// Returns UTF-8
		virtual const char*	get_variable(const char* path_to_var) const = 0;
		// @@ do we want a version that returns a number?

		// Executes a command containing a method name and quoted or numeric arguments.
		// The wide form takes UTF-16 units. The result is shared, ephemeral text.
		virtual const char* call_method(const unsigned short* expression) = 0;
		virtual const char* call_method(const char* expression) = 0;



		// Make the movie visible/invisible.  An invisible
		// movie does not advance and does not render.
		virtual void	set_visible(bool visible) = 0;

		// Return visibility status.
		virtual bool	get_visible() const = 0;

		// Set and get userdata, that's useful for the fs_command handler.
		virtual void   *get_userdata() = 0;
		virtual void   set_userdata(void *) = 0;

		// for external movies
		virtual movie*	get_root_movie() = 0;
	};

	// Try to grab movie info from the header of the given .swf
	// file.
	//
	// Sets *version to 0 if info can't be extracted.
	//
	// You can pass NULL for any entries you're not interested in.
	void	get_movie_info(
		const char*	filename,
		int*		version,
		int*		width,
		int*		height,
		float*		frames_per_second,
		int*		frame_count,
		int*		tag_count
		);

	// Enable/disable attempts to read cache files (.gsc) when
	// loading movies.
	void	set_use_cache_files(bool use_cache);
	
	// @@ Hm, need to think about these creation API's.  Perhaps
	// divide it into "low level" and "high level" calls.  Also,
	// perhaps we need a "context" object that contains all
	// global-ish flags, libraries, callback pointers, font
	// library, etc.
	//
	// Create a gameswf::movie_definition from the given file name.
	// Normally, will also try to load any cached data file
	// (".gsc") that corresponds to the given movie file.  This
	// will still work even if there is no cache file.  You can
	// disable the attempts to load cache files by calling
	// gameswf::use_cache_files(false).
	//
	// Uses the registered file-opener callback to read the files
	// themselves.
	//
	// This calls add_ref() on the newly created definition; call
	// drop_ref() when you're done with it.
	// Or use smart_ptr<T> from base/smart_ptr.h if you want.
	movie_definition*	create_movie(const char* filename);

	// Creates the movie from the given input stream.  Only reads
	// from the given stream; does not open files.  If the movie
	// imports resources from other movies, the created movie
	// inserts proxy stubs in place of those resources.  The list
	// of imported movie filenames can be retrieved with
	// movie_definition::visit_imported_movies().  The proxies can
	// be replaced with actual movie_definition's via
	// movie_definition::resolve_proxy(name,def).
	//
	// Use DO_NOT_LOAD_BITMAPS if you have pre-processed bitmaps
	// stored externally somewhere, and you plan to install them
	// via get_bitmap_info()->...
	enum create_bitmaps_flag
	{
		DO_LOAD_BITMAPS,
		DO_NOT_LOAD_BITMAPS
	};
	// Use DO_NOT_LOAD_FONT_SHAPES if you know you have
	// precomputed texture glyphs (in cached data) and you know
	// you always want to render text using texture glyphs.
	enum create_font_shapes_flag
	{
		DO_LOAD_FONT_SHAPES,
		DO_NOT_LOAD_FONT_SHAPES
	};
	movie_definition*	create_movie_no_recurse(
		tu_file*		input_stream,
		create_bitmaps_flag	cbf,
		create_font_shapes_flag cfs);

	// [CHOSEN, not recovered] Legacy native factory-policy adapter. The
	// SwiffPlayer keeps generation disabled until cached generated textures can
	// rebind to a new renderer (swf-cached-bitmap-renderer-ownership). Retail's
	// factory enables generation; image tags still construct named descriptors.
	void	set_default_create_bitmaps(create_bitmaps_flag cbf);

	// Create a gameswf::movie_definition from the given file name.
	// This is just like create_movie(), except that it checks the
	// "library" to see if a movie of this name has already been
	// created, and returns that movie if so.  Also, if it creates
	// a new movie, it adds it back into the library.
	//
	// The "library" is used when importing symbols from external
	// movies, so this call might be useful if you want to
	// explicitly load a movie that you know exports symbols
	// (e.g. fonts) to other movies as well.
	//
	// @@ this explanation/functionality could be clearer!
	//
	// This calls add_ref() on the newly created definition; call
	// drop_ref() when you're done with it.
	// Or use smart_ptr<T> from base/smart_ptr.h if you want.
	movie_definition*	create_library_movie(const char* filename);
	

	// Helper to pregenerate cached data (basically, shape
	// tesselations).  Does this by running through each frame of
	// the movie and displaying the shapes with a null renderer.
	// The pregenerated data is stored in the movie_definition
	// object itself, and is included with the cached data written
	// by movie_definition::output_cached_data().
	//
	// Note that this tesselates shapes to the resolution they
	// explicitly appear in the linear frames of the movie.  Does
	// not try very hard to run your ActionScript to account for
	// dynamic scaling (that's more or less futile anyway due to
	// the halting problem).
	void	precompute_cached_data(movie_definition* movie_def);

	// Maximum release of resources.  Calls clear_library() and
	// fontlib::clear(), and also clears some extra internal stuff
	// that may have been allocated (e.g. global ActionScript
	// objects).  This should get all gameswf structures off the
	// heap, with the exception of any objects that are still
	// referenced by the host program and haven't had drop_ref()
	// called on them.
	void	clear();


	//
	// Library management
	//
	
	// Release any library movies we've cached.  Do this when you want
	// maximum cleanup.
	void	clear_library();
	
	//
	// Font library control.  gameswf is able to substitute fonts
	// from the font library, in case a movie lacks glyphs for a
	// declared font.  This would come into play since in recent
	// versions of SWF, the movie is allowed to use "system
	// fonts".  E.g. it can declare a font named "Arial", but not
	// provide glyphs for it, and then the OS is expected to
	// provide the font or a suitable replacement.
	//
	// gameswf does not try to handle this automatically; if your
	// host program wants to emulate this behavior, it needs to
	// load a movie that includes glyph info for the standard
	// fonts you want, and then explicitly pull those fonts out of
	// the movie_def and add them to fontlib.
	//
	// @@ TODO: not all public APIs to enable this are in place
	// yet!  Need md::get_font_count()/get_font(), and
	// fontlib::add_font().
	//
	// Otherwise, text written in a font with no glyphs just
	// doesn't render at all.  (@@ Hm, should probably render it
	// as boxes or something?)

	struct font;
	namespace fontlib
	{
		// Controls how large to render textured glyphs.
		// Applies to fonts processed *after* this call only.
		// The "nominal" size is perhaps around twice the
		// average glyph height.
		void	set_nominal_glyph_pixel_size(int pixel_size);

		// For accessing the fonts in the library.
		void	clear();
		int	get_font_count();
		font*	get_font(int index);
		font*	get_font(const char* name);
		const char*	get_font_name(const font* f);
		
		// @@ also need to add color controls (or just set the diffuse color
		// in the API?), perhaps matrix xform, and maybe spacing, etc.
		//
		// // For direct text rendering from the host app.
		void	draw_string(const font* f, float x, float y, float size, const char* text);
		// void	draw_string(const font* f, float x, float y, float size, const wchar_t* text);	// wide-char version
	}
	
	
	//
	// Sound callback handler.
	//
	
	// You may define a subclass of this, and pass an instance to
	// set_sound_handler().
	struct sound_handler
	{
		enum format_type
		{
			FORMAT_RAW = 0,		// unspecified format.  Useful for 8-bit sounds???
			FORMAT_ADPCM = 1,	// gameswf doesn't pass this through; it uncompresses and sends FORMAT_NATIVE16
			FORMAT_MP3 = 2,
			FORMAT_UNCOMPRESSED = 3,	// 16 bits/sample, little-endian
			FORMAT_NELLYMOSER = 6,	// Mystery proprietary format; see nellymoser.com
				
			// gameswf tries to convert data to this format when possible:
			FORMAT_NATIVE16 = 7	// gameswf extension: 16 bits/sample, native-endian
		};
		// If stereo is true, samples are interleaved w/ left sample first.
		
		// gameswf calls at load-time with sound data, to be
		// played later.  You should create a sample with the
		// data, and return a handle that can be used to play
		// it later.  If the data is in a format you can't
		// deal with, then you can return 0 (for example), and
		// then ignore 0's in play_sound() and delete_sound().
		//
		// Assign handles however you like.
		virtual int	create_sound(
			void*		data,
			int		data_bytes,
			int		sample_count,
			format_type	format,
			int		sample_rate,	/* one of 5512, 11025, 22050, 44100 */
			bool		stereo
			) = 0;
		
		// gameswf calls this when it wants you to play the defined sound.
		//
		// loop_count == 0 means play the sound once (1 means play it twice, etc)
		virtual void	play_sound(int sound_handle, int loop_count /* , volume, pan, etc? */) = 0;
		
		// Stop the specified sound if it's playing.
		// (Normally a full-featured sound API would take a
		// handle specifying the *instance* of a playing
		// sample, but SWF is not expressive that way.)
		virtual void	stop_sound(int sound_handle) = 0;
		
		// gameswf calls this when it's done with a particular sound.
		virtual void	delete_sound(int sound_handle) = 0;
		
		virtual ~sound_handler() {};
	};
	

	//
	// matrix type, used by render handler
	//

	struct point;
	struct matrix
	{
		float	m_[2][3];

		static matrix	identity;

		matrix();
		bool	is_valid() const;
		void	set_identity();
		void	concatenate(const matrix& m);
		void	concatenate_translation(float tx, float ty);
		void	concatenate_scale(float s);
		void	set_lerp(const matrix& m1, const matrix& m2, float t);
		void	set_scale_rotation(float x_scale, float y_scale, float rotation);
		void	read(stream* in);
		void	print() const;
		void	transform(point* result, const point& p) const;
		void	transform_vector(point* result, const point& p) const;
		void	transform_by_inverse(point* result, const point& p) const;
		void	set_inverse(const matrix& m);
		bool	does_flip() const;	// return true if we flip handedness
		float	get_determinant() const;	// determinant of the 2x2 rotation/scale part only
		float	get_max_scale() const;	// return the maximum scale factor that this transform applies
		float	get_x_scale() const;	// return the magnitude scale of our x coord output
		float	get_y_scale() const;	// return the magnitude scale of our y coord output
		float	get_rotation() const;	// return our rotation component (in radians)
	};


	//
	// point: used by rect which is used by render_handler (otherwise would be in internal gameswf_types.h)
	//


	struct point
	{
		float	m_x, m_y;

		// SwiffPlayer.dll 0x1006EB70.
		point() : m_x(0), m_y(0) {}
		point(float x, float y) : m_x(x), m_y(y) {}

		void	set_lerp(const point& a, const point& b, float t)
		// Set to a + (b - a) * t
		{
			m_x = a.m_x + (b.m_x - a.m_x) * t;
			m_y = a.m_y + (b.m_y - a.m_y) * t;
		}

		bool operator==(const point& p) const { return m_x == p.m_x && m_y == p.m_y; }

		bool	bitwise_equal(const point& p) const;
	};


	//
	// rect: rectangle type, used by render handler
	//


	struct rect
	{
		float	m_x_min, m_x_max, m_y_min, m_y_max;

		void	read(stream* in);
		void	print() const;
		bool	point_test(float x, float y) const;
		void	expand_to_point(float x, float y);
		// SwiffPlayer.dll 0x1006EBA0 / 0x1006EBB0.
		float	width() const { return m_x_max-m_x_min; }
		float	height() const { return m_y_max-m_y_min; }

		point	get_corner(int i) const;

		void	enclose_transformed_rect(const matrix& m, const rect& r);
		// Transforms only the minimum and maximum corners, without sorting them.
		// [INFERRED] Source-level name; SwiffPlayer.dll 0x1009E410.
		void transform(const matrix& m);
		// Includes the other rectangle's minimum corner, then its maximum corner.
		// [INFERRED] Source-level name; SwiffPlayer.dll 0x1009E780.
		void expand_to_rect(const rect& r);

		void	set_lerp(const rect& a, const rect& b, float t);
	};


	//
	// cxform: color transform type, used by render handler
	//
	struct cxform
	{
		float	m_[4][2];	// [RGBA][mult, add]

		cxform();
		void	concatenate(const cxform& c);
		rgba	transform(const rgba in) const;
		void	read_rgb(stream* in);
		void	read_rgba(stream* in);
		void	clamp();  // Force component values to be in range.
		void	print() const;

		static cxform	identity;
	};


	//
	// texture and render callback handler.
	//

	// Your render_handler creates bitmap_info's for gameswf.  You
	// need to subclass bitmap_info in order to add the
	// information and functionality your app needs to render
	// using textures.
	struct bitmap_info : public ref_counted
	{
		// [CHOSEN, not recovered] Native cached images may outlive their GPU
		// registry (issue #7). Such images own their release/rebinding state.
		struct render_resource { virtual ~render_resource() {} };
		std::unique_ptr<render_resource> m_render_resource;

		// [INFERRED] name: primary slot 0 distinguishes named descriptors.
		// SwiffPlayer.dll 0x10004EB0, consumed at 0x10006311.
		virtual bool is_named() const { return false; }
		virtual ~bitmap_info();
		unsigned int	m_texture_id;		// nuke?
		int		m_original_width;	// nuke?
		int		m_original_height;	// nuke?
		
		bitmap_info()
			:
			m_texture_id(0),
			m_original_width(0),
			m_original_height(0)
		{
		}
	};
	
	// A movie bitmap whose ExportAssets symbol names its external texture.
	// SwiffPlayer.dll 0x10071930; destruction 0x10073B40/0x10073B10 destroys
	// the string before the bitmap_info base notifies the renderer.
	struct named_bitmap_info : public bitmap_info
	{
		// SwiffPlayer.dll 0x100719A0.
		bool is_named() const override { return true; }
		// [RETIRE-WHEN: swf-native-bitmap-name] Storage is now descriptor-owned;
		// maintainer exported-menu-image/reload acceptance remains required.
		std::string m_name;
	};

	// You must define a subclass of render_handler, and pass an
	// instance to set_render_handler().
	// the focused text field's keyboard/clipboard surface - the shipped
	// +200 interface of DICE's edit_text_character, the object dword_100F8EE0 points at. SwiffPlayer's
	// key latch, notifyChar, setTextInput and getTextInput0/1 dispatch into it (slots 0..4).
	struct text_input
	{
		// Implicit vptr construction: SwiffPlayer.dll 0x10084D80.
		// Destruction clears focus after the field's members: 0x10084D90.
		virtual ~text_input();
		virtual void	on_key(int vkey) = 0;
		virtual void	on_char(int character) = 0;
		virtual void	paste() = 0;
		virtual void	copy() = 0;
		virtual void	cut() = 0;
	};
	text_input*	get_focused_text_input();
	void	set_text_input_modifiers(bool shift, bool alt, bool ctrl, bool caps);
	tu_string&	text_input_clipboard();	// the shipped `src` string

	struct render_handler
	{
		virtual ~render_handler() {}

		// Your handler should return these with a ref-count of 0.  (@@ is that the right policy?)
		virtual bitmap_info*	create_bitmap_info_empty() = 0;	// used when DO_NOT_LOAD_BITMAPS is set
		virtual bitmap_info*	create_bitmap_info_alpha(int w, int h, unsigned char* data) = 0;
		virtual bitmap_info*	create_bitmap_info_rgb(image::rgb* im) = 0;
		virtual bitmap_info*	create_bitmap_info_rgba(image::rgba* im) = 0;
		// the 2004 base's fifth slot, dropped by 2006 - a bitmap resolved BY
		// FILE NAME through the host (BF2: ISwiffRender::loadTexture). The sprite `loadMovie`
		// image loader (gameswf_sprite_loadmovie -> render::create_bitmap_info_file, sub_1009A620)
		// hangs every `loadImg` on it. The native handler keeps failed texture handles.
		virtual bitmap_info*	create_bitmap_info_file(const char* path) { return NULL; }

		// the twelve text slots DICE's edit_text_character drives - a
		// renderer-side text LINE object per laid-out line. Defaults do nothing (no renderer, no text),
		// the SwiffPlayer handler forwards them to ISwiffRender. See GAMESWF_DIVERGENCES.md.
		virtual void*	text_create(const char* /*font_name*/, int /*height_px*/) { return NULL; }
		virtual void	text_release(void* /*line*/) {}
		virtual void	text_set(void* /*line*/, const wchar_t* /*text*/) {}
		virtual void	text_detach(void* /*line*/) {}
		virtual void	text_draw(void* /*line*/, const matrix* /*m*/, unsigned /*color*/, int /*flags*/) {}
		virtual bool	text_extent(void* /*line*/, int /*count*/, float* /*w*/, float* /*h*/) { return false; }
		//  float coordinates: SwiffPlayer.dll 0x100887A5..0x100887D7 / 0x10088823..0x1008883B
		virtual bool	text_hit(void* /*line*/, int* /*out_pos*/, float /*x*/, float /*y*/) { return false; }
		virtual bool	text_selection(void* /*line*/, int* a, int* b) { if (a) *a = 0; if (b) *b = 0; return false; }	//  true = a selection exists
		virtual bool	text_set_selection(void* /*line*/, float, float, float, float) { return false; }
		virtual void	text_blur(void* /*line*/) {}
		virtual void	text_set_size(void* /*line*/, float /*size*/, int /*flag*/) {}
		virtual void	text_caret(void* /*line*/, int /*pos*/) {}

		virtual void	delete_bitmap_info(bitmap_info* bi) = 0;
		
		// Bracket the displaying of a frame from a movie.
		// Fill the background color, and set up default
		// transforms, etc.
		virtual void	begin_display(
			rgba background_color,
			int viewport_x0, int viewport_y0,
			int viewport_width, int viewport_height,
			float x0, float x1, float y0, float y1) = 0;
		virtual void	end_display() = 0;
		
		// Geometric and color transforms for mesh and line_strip rendering.
		virtual void	set_matrix(const matrix& m) = 0;
		virtual void	set_cxform(const cxform& cx) = 0;
		// Submit the display-list depth before mesh and line callbacks.
		// SwiffRenderHandler (SwiffPlayer.h) is the one backend; every handler supplies depth.
		virtual void	set_depth(int depth) = 0;
		
		// Draw triangles using the current fill-style 0.
		// Clears the style list after rendering.
		//
		// coords is a list of (x,y) coordinate pairs, in
		// triangle-strip order.  The type of the array should
		// be Sint16[vertex_count*2]
		virtual void	draw_mesh_strip(const void* coords, int vertex_count) = 0;
		
		// Draw a line-strip using the current line style.
		// Clear the style list after rendering.
		//
		// Coords is a list of (x,y) coordinate pairs, in
		// sequence.  Each coord is a 16-bit signed integer.
		virtual void	draw_line_strip(const void* coords, int vertex_count) = 0;
		
		// Set line and fill styles for mesh & line_strip
		// rendering.
		enum bitmap_wrap_mode
		{
			WRAP_REPEAT,
			WRAP_CLAMP
		};
		virtual void	fill_style_disable(int fill_side) = 0;
		virtual void	fill_style_color(int fill_side, rgba color) = 0;
		virtual void	fill_style_bitmap(int fill_side, const bitmap_info* bi, const matrix& m, bitmap_wrap_mode wm) = 0;
		
		virtual void	line_style_disable() = 0;
		virtual void	line_style_color(rgba color) = 0;
		virtual void	line_style_width(float width) = 0;
		
		// Special function to draw a rectangular bitmap;
		// intended for textured glyph rendering.  Ignores
		// current transforms.
		virtual void	draw_bitmap(
			const matrix&		m,
			const bitmap_info*	bi,
			const rect&		coords,
			const rect&		uv_coords,
			rgba			color) = 0;
		
		virtual void	set_antialiased(bool enable) = 0;
		
		virtual void begin_submit_mask() = 0;
		virtual void end_submit_mask() = 0;
		virtual void disable_mask() = 0;
	};
	
	// Keyboard handling
	// [CHOSEN, not recovered] Native input adapter. Query consumes the low press
	// bit and returns the Win32 asynchronous high/low bit representation.
	typedef unsigned short (*async_key_state_callback)(int key_code);
	void set_async_key_state_callback(async_key_state_callback callback);
	bool is_key_down(int key_code);

	namespace key {
		enum code
		{
			INVALID = 0,
			A = 65,
			B,
			C,
			D,
			E,
			F,
			G,
			H,
			I,
			J,
			K,
			L,
			M,
			N,
			O,
			P,
			Q,
			R,
			S,
			T,
			U,
			V,
			W,
			X,
			Y,
			Z,
			_0 = 48,
			_1,
			_2,
			_3,
			_4,
			_5,
			_6,
			_7,
			_8,
			_9,
			KP_0 = 96,
			KP_1,
			KP_2,
			KP_3,
			KP_4,
			KP_5,
			KP_6,
			KP_7,
			KP_8,
			KP_9,
			KP_MULTIPLY,
			KP_ADD,
			KP_ENTER,
			KP_SUBTRACT,
			KP_DECIMAL,
			KP_DIVIDE,
			F1 = 112,
			F2,
			F3,
			F4,
			F5,
			F6,
			F7,
			F8,
			F9,
			F10,
			F11,
			F12,
			F13,
			F14,
			F15,
			BACKSPACE = 8,
			TAB,
			CLEAR = 12,
			ENTER,
			SHIFT = 16,
			CONTROL,
			ALT,
			CAPSLOCK = 20,
			ESCAPE = 27,
			SPACE = 32,
			PGDN,
			PGUP,
			END = 35,
			HOME,
			LEFT,
			UP,
			RIGHT,
			DOWN,
			INSERT = 45,
			DELETEKEY,
			HELP,
			NUM_LOCK = 144,
			SEMICOLON = 186,
			EQUALS = 187,
			MINUS = 189,
			SLASH = 191,
			BACKTICK = 192,
			LEFT_BRACKET = 219,
			BACKSLASH = 220,
			RIGHT_BRACKET = 221,
			QUOTE = 222,

			KEYCOUNT
		};
	}	// end namespace key

	// Key events are global throughout gameswf.
	// @@ Maybe someday make these local to the movie_interface?
	void	notify_key_event(key::code k, bool down);


}	// namespace gameswf


#endif // GAMESWF_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
