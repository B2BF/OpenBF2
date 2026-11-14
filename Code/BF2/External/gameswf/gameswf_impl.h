// gameswf_impl.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some implementation code for the gameswf SWF player library.


#ifndef GAMESWF_IMPL_H
#define GAMESWF_IMPL_H


#include "gameswf.h"
#include "gameswf_action.h"
#include "gameswf_types.h"
#include "gameswf_log.h"
#include "../../Platform/X87Compat.h"
#include <assert.h>
#include "base/container.h"
#include "base/utility.h"
#include "base/smart_ptr.h"
#include <stdarg.h>


namespace jpeg { struct input; }


namespace gameswf
{
	struct action_buffer;
	struct bitmap_character_def;
        struct bitmap_info;
	struct character;
	struct character_def;
	struct display_info;
	struct execute_tag;
	struct font;
	struct movie_root;
	struct sound_sample : public resource //virtual public ref_counted
	{
		// SwiffPlayer.dll 0x1009E860 constructs only the inherited resource;
		// 0x1009E8D0 / 0x1009F200 and deleting wrapper 0x1009F1D0 destroy it.
		// resource carries the shared virtual ref_counted base, as retail (game-frontier C1).
		// SwiffPlayer.dll 0x1009E8C0; resource table 0x100E2B88 +0x08.
		virtual sound_sample*	cast_to_sound_sample() { return this; }
	};
	struct stream;
	struct swf_event;

	void save_extern_movie(movie_interface* m);
	// SwiffPlayer.dll 0x1006F110; [INFERRED] name of the mouse-event verbosity switch.
	void set_verbose_mouse_events(bool verbose);
	// Enables movie and renderer timing. Counters retain their individual retail gates.
	// SwiffPlayer.dll 0x1006F140; [INFERRED] source-level name.
	void set_profiling(bool enabled);

	// Extra internal interfaces added to movie_definition
	struct movie_definition_sub : public movie_definition
	{
		// Compiler deleting wrapper: SwiffPlayer.dll 0x10073530; base lifetime is separate.
		~movie_definition_sub() override = default;
		virtual const array<execute_tag*>&	get_playlist(int frame_number) = 0;
		virtual const array<execute_tag*>*	get_init_actions(int frame_number) = 0;
		virtual smart_ptr<resource>	get_exported_resource(const tu_string& symbol) = 0;
		// SwiffPlayer.dll 0x10078260: looks up the exported name by resource ID.
		// A null output pointer tests presence without copying the name.
		virtual bool get_export_name(int id, tu_string* out) = 0;
		// [CHOSEN, not recovered] S3 bridge: retained legacy pointer-to-ID query.
		// Text construction now uses get_export_name with the declared font ID.
		// [RETIRE-WHEN: swf-text-font-id] Remove this unused native virtual after
		// exported/aliased fonts pass the maintainers' menu gates.
		virtual bool	get_exported_font_name(const font* f, tu_string* out) { return false; }
		virtual character_def*	get_character_def(int id) = 0;
		// Movie teardown: drops the classes Object.registerClass bound to this definition's
		// exports, so a definition kept in the library for the next player holds no function of
		// the released movie. [CHOSEN, not recovered] issue #7; see release_script_objects.
		virtual void	release_registered_classes() {}

		virtual bool	get_labeled_frame(const char* label, int* frame_number) = 0;

		// For use during creation.
		virtual int	get_loading_frame() const = 0;
		virtual void	add_character(int id, character_def* ch) = 0;
		virtual void	add_font(int id, font* ch) = 0;
		virtual font*	get_font(int id) = 0;
		virtual void	add_execute_tag(execute_tag* c) = 0;
		// SwiffPlayer.dll 0x10078F90 / 0x10071630: definition-wide append or refusal.
		virtual void	add_init_action(execute_tag* c) = 0;
		virtual void	add_frame_name(const char* name) = 0;
		virtual void	set_jpeg_loader(jpeg::input* j_in) = 0;
		virtual jpeg::input*	get_jpeg_loader() = 0;
		virtual bitmap_character_def*	get_bitmap_character(int character_id) = 0;
		virtual void	add_bitmap_character(int character_id, bitmap_character_def* ch) = 0;
		virtual sound_sample*	get_sound_sample(int character_id) = 0;
		virtual void	add_sound_sample(int character_id, sound_sample* sam) = 0;
		virtual void	export_resource(int id, const tu_string& symbol, resource* res) = 0;
		virtual void	add_import(const char* source_url, int id, const char* symbol_name) = 0;
		virtual void	add_bitmap_info(bitmap_info* ch) = 0;

		virtual create_bitmaps_flag	get_create_bitmaps() const = 0;
		virtual create_font_shapes_flag	get_create_font_shapes() const = 0;
	};


	// For internal use.
	movie_definition_sub*	create_movie_sub(const char* filename);
	movie_definition_sub*	create_library_movie_sub(const char* filename);
	movie_interface*	create_library_movie_inst_sub(movie_definition_sub* md);

//v for extern movies
	movie_interface*	create_library_movie_inst(movie_definition* md);
	movie_interface*        get_current_root();
	void set_current_root(movie_interface* m);
	// The currently published root sprite, or null.
	// This is independent of the external-movie interface selected above.
	movie* get_global_root_movie();
	// Replaces the retained root; an identical pointer leaves ownership unchanged.
	void set_global_root_movie(movie* root);
	const char* get_workdir();
	void set_workdir(const char* dir);
	void delete_unused_root();

	struct movie : public movie_interface
	{
		// [CHOSEN, not recovered] S2 bridge for the retail secondary-interface
		// environment lookup, expressed through the native movie interface.
		virtual as_environment*	get_action_environment() = 0;
		// SwiffPlayer.dll 0x1007DB90 / 0x1007DBA0 (release defaults).
		virtual void set_extern_movie(movie_interface* m) { }
		virtual movie_interface*	get_extern_movie() { return NULL; }

		// SwiffPlayer.dll 0x1007DBC0 / 0x1007DBE0 / 0x1007DBD0 (release defaults).
		virtual movie_definition*	get_movie_definition() { return NULL; }
		virtual movie_root*	        get_root() { return NULL; }
		virtual movie_interface*	get_root_interface() { return NULL; }
		virtual movie*	                get_root_movie() { return NULL; }

		// SwiffPlayer.dll 0x1007DBF0.
		virtual float	                get_pixel_scale() const { return 1.0f; }
		// SwiffPlayer.dll 0x1007DC00 (release default).
		virtual character*	        get_character(int id) { return NULL; }
		// a SAFE downcast to character, the cast_to_font / cast_to_as_object
		// pattern. The Color builtin's setRGB needs its target's cxform (see gameswf_action.cpp).
		virtual character*	        cast_to_character() { return NULL; }

		// SwiffPlayer.dll 0x1007DC10 / 0x1007DC60.
		virtual matrix	                get_world_matrix() const { return matrix::identity; }
		virtual cxform	                get_world_cxform() const { return cxform::identity; }

		//
		// display-list management.
		//

		virtual execute_tag*	find_previous_replace_or_add_tag(int current_frame, int depth, int id)
		{
			return NULL;
		}

		// SwiffPlayer.dll 0x1007DC80 returns NULL without examining its arguments.
		virtual character*	add_display_object(
			Uint16 character_id,
			const char*		 name,
			const array<swf_event*>& event_handlers,
			Uint16			 depth,
			bool			 replace_if_depth_is_occupied,
			const cxform&		 color_transform,
			const matrix&		 mat,
			float			 ratio,
			Uint16			clip_depth)
		{
			return NULL;
		}

		// SwiffPlayer.dll 0x1007DC90 is empty.
		virtual void	move_display_object(
			Uint16		depth,
			bool		use_cxform,
			const cxform&	color_transform,
			bool		use_matrix,
			const matrix&	mat,
			float		ratio,
			Uint16		clip_depth)
		{
		}

		// SwiffPlayer.dll 0x1007DCA0 / 0x1007DCB0 are empty overloads.
		virtual void	replace_display_object(
			Uint16		character_id,
			const char*	name,
			Uint16		depth,
			bool		use_cxform,
			const cxform&	color_transform,
			bool		use_matrix,
			const matrix&	mat,
			float		ratio,
			Uint16		clip_depth)
		{
		}

		virtual void	replace_display_object(
			character*	ch,
			const char*	name,
			Uint16		depth,
			bool		use_cxform,
			const cxform&	color_transform,
			bool		use_matrix,
			const matrix&	mat,
			float		ratio,
			Uint16		clip_depth)
		{
		}

		// SwiffPlayer.dll 0x1007DCC0 is empty; the native ID argument is
		// retained for tag 5 [CHOSEN, not recovered] S3 bridge.
		virtual void	remove_display_object(Uint16 depth)	{}

		// SwiffPlayer.dll 0x1007DD00 / 0x1007DCD0: empty setter and transparent black.
		virtual void	set_background_color(const rgba& color) {}
		virtual rgba	get_background_color() const { return rgba(0, 0, 0, 0); }
		// SwiffPlayer.dll 0x1007DD10 / 0x1007DD20.
		virtual void	set_background_alpha(float alpha) {}
		virtual float	get_background_alpha() const { return 1.0f; }
		// SwiffPlayer.dll 0x1007DD30 is empty.
		virtual void	set_display_viewport(int x0, int y0, int width, int height) {}

		// SwiffPlayer.dll 0x1007DD40 is empty in the release image.
		virtual void	add_action_buffer(action_buffer* a) { assert(0); }

		// SwiffPlayer.dll 0x1007DD50 is empty in the release image.
		virtual void	goto_frame(int target_frame_number) { assert(0); }
		// SwiffPlayer.dll 0x1007DD70: immediate numeric navigation is separate
		// from queued goto_frame. The release movie default does nothing.
		virtual void	apply_goto_frame(int target_frame_number) {}
		// SwiffPlayer.dll 0x1007DD60; sprites override the queued label entry.
		virtual void	queue_goto_labeled_frame(const char* label) {}
		// SwiffPlayer.dll 0x1007DD80 is empty; the native Boolean result is an
		// existing [CHOSEN, not recovered] S3 bridge for the 2006 interface.
		virtual bool	goto_labeled_frame(const char* label) { assert(0); return false; }

		// SwiffPlayer.dll 0x1007DD90 / 0x1007DDA0 (release defaults).
		virtual void	set_play_state(play_state s) {}
		virtual play_state	get_play_state() const { assert(0); return STOP; }

		// SwiffPlayer.dll 0x1007DDB0 is empty.
		virtual void	notify_mouse_state(int x, int y, int buttons)
		// The host app uses this to tell the movie where the
		// user's mouse pointer is.
		{
		}

		// SwiffPlayer.dll 0x1007DDC0 leaves all three outputs alone.
		virtual void	get_mouse_state(int* x, int* y, int* buttons)
		// Use this to retrieve the last state of the mouse, as set via
		// notify_mouse_state().
		{
			assert(0);
		}

		struct drag_state
		{
			movie*	m_character;
			bool	m_lock_center;
			bool	m_bound;
			float	m_bound_x0;
			float	m_bound_y0;
			float	m_bound_x1;
			float	m_bound_y1;
			// [CHOSEN, not recovered] S2 bridge fields for retail's float drag offsets
			// (+0x18/+0x1C in the x86 record). 0x10092F3A/0x10092F40 write them,
			// 0x100733E5/0x100733EE read them. Publishers provide completed records.
			float	m_offset_x = 0;
			float	m_offset_y = 0;

			// SwiffPlayer.dll 0x1006ED60 initializes the six original fields only.
			drag_state()
				:
				m_character(0), m_lock_center(0), m_bound(0),
				m_bound_x0(0), m_bound_y0(0), m_bound_x1(1), m_bound_y1(1)
			{
			}
		};
		// SwiffPlayer.dll 0x1007DDF0 / 0x1007DE30 / 0x1007DE40.
		// [CHOSEN, not recovered] S3 bridge: the native record initializes its
		// offset fields; the retail default copies uninitialized trailing words.
		virtual void	get_drag_state(drag_state* st) { assert(0); *st = drag_state(); }
		// Publish completed world-twip bounds and drag-local offsets without conversion.
		virtual void	set_drag_state(const drag_state& st) { assert(0); }
		virtual void	stop_drag() { assert(0); }


		// SwiffPlayer.dll 0x1007DE50 is empty.
		// External
		virtual void	set_variable(const char* path_to_var, const char* new_value)
		{
			assert(0);
		}

		// SwiffPlayer.dll 0x1007DE60 is empty.
		// External
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value)
		{
			assert(0);
		}

		// SwiffPlayer.dll 0x1007DE70 returns the empty string.
		// External
		virtual const char*	get_variable(const char* path_to_var) const
		{
			assert(0);
			return "";
		}

		// SwiffPlayer.dll 0x1007DE80 / 0x1007DE90 (release defaults).
		virtual void * get_userdata() { assert(0); return NULL; }
		virtual void set_userdata(void *) { assert(0); }

		// External
		// SwiffPlayer.dll 0x1007DEA0.
		virtual bool	has_looped() const { return true; }


		//
		// Mouse/Button interface.
		//

		//  The shipped player has no "topmost mouse entity" pass. Every
		// character runs its own per-frame mouse state machine (vtable slot 4, mouse_update): the root
		// sprite is asked once per advance, a sprite handles itself and then every visible child from
		// the top down, and a child's answer - "I consumed the pointer" - is what the children below it
		// see. Consumption comes only from sprites that carry a mouse handler (m_mouse_enabled) and from
		// text fields; a bare shape, a mask or a decorative clip never claims the pointer, which is how
		// a click on a Textbox's bevel still reaches its edit text and a click beside a closed dropdown
		// still reaches the button under its list mask. Slot 61, point_test_world, is the geometric
		// half: does this character cover the world-space point (twips). The character defaults are
		// the shipped 0x1007EEF0 / 0x1007EF00, both false.
		// SwiffPlayer.dll 0x1007EEF0 / 0x1007EF00.
		virtual bool	mouse_update(bool consumed) { return false; }
		virtual bool	point_test_world(float x, float y) { return false; }
		// The root's mouse capture (the shipped movie_root's +0xB4 / +0xB8): the character that took a
		// press holds the pointer until it releases. Sprites forward to their movie_root.
		// SwiffPlayer.dll 0x1007DDD0 / 0x1007DDE0.
		virtual movie*	get_active_mouse_entity() { return NULL; }
		virtual void	set_active_mouse_entity(movie* m) {}
		virtual bool	get_track_as_menu() const { return false; }


		//
		// ActionScript.
		//


		// SwiffPlayer.dll 0x1007DEB0, movie secondary tables +0x1C.
		virtual movie*	get_relative_target(const tu_string& name)
		{
			assert(0);	
			return NULL;
		}

		// ActionScript event handler.  Returns true if a handler was called.
		// SwiffPlayer.dll 0x1007DEC0.
		virtual bool	on_event(event_id id) { return false; }

		// Special event handler; sprites also execute their frame1 actions on this event.
		// SwiffPlayer.dll 0x1007DED0; native event IDs map LOAD to retail 10.
		virtual void	on_event_load() { on_event(event_id::LOAD); }

		// as_object_interface stuff
		// SwiffPlayer.dll 0x1007DEF0 / 0x1007DF00 (release defaults).
		virtual void	set_member(const tu_stringi& name, const as_value& val) { assert(0); }
		virtual bool	get_member(const tu_stringi& name, as_value* val) { assert(0); return false; }

		// SwiffPlayer.dll 0x1007DF20 is empty in the release image.
		virtual void	call_frame_actions(const as_value& frame_spec) { assert(0); }

		// SwiffPlayer.dll 0x1007DF30 / 0x1007DF40.
		virtual float	get_timer() const { return 0.0f; }
		virtual movie*	to_movie() { return this; }

		// SwiffPlayer.dll 0x1007DF60 / 0x1007DF70 (release defaults).
		virtual void	clone_display_object(const tu_string& name, const tu_string& newname, Uint16 depth) { assert(0); }
		virtual void	remove_display_object(const tu_string& name) { assert(0); }

		// SwiffPlayer.dll 0x1007EFC0 / 0x1007EFB0; characters without
		// a script environment return null for either command width.
		virtual const char* call_method(const Uint16* expression) { return NULL; }
		virtual const char* call_method(const char* expression) { return NULL; }

		virtual void	execute_frame_tags(int frame, bool state_only = false) {}

		// SwiffPlayer.dll 0x1007ED80: characters without editable text ignore this.
		// Sprites forward it to referenced children; text fields update tab order.
		virtual void	set_tab_index(int index) {}
		// SwiffPlayer.dll 0x1007DBB0: absent outside sprites.
		virtual int	get_tab_index() const { return -1; }
	};


	// Information about how to display a character.
	struct display_info
	{
		movie*	m_parent;
		int	m_depth;
		cxform	m_color_transform;
		matrix	m_matrix;
		float	m_ratio;
                Uint16 	m_clip_depth;

		display_info()
			:
			m_parent(NULL),
			m_depth(0),
			m_ratio(0.0f),
                        m_clip_depth(0)
		{
		}

		void	concatenate(const display_info& di)
		// Concatenate the transforms from di into our
		// transforms.
		{
			m_depth = di.m_depth;
			m_color_transform.concatenate(di.m_color_transform);
			m_matrix.concatenate(di.m_matrix);
			m_ratio = di.m_ratio;
                        m_clip_depth = di.m_clip_depth;
		}
	};


	// character is a live, stateful instance of a character_def.
	// It represents a single active element in a movie.
	struct character : public movie
	{
		// Compiler deleting wrapper: SwiffPlayer.dll 0x10084AE0. The character
		// lifetime body at 0x1007F0A0 remains a separate recovery boundary.
		~character() override;
		// SwiffPlayer.dll 0x1007EC60 constructs an Array at character +0x1C;
		// 0x10080CEE exposes it through the movie's "arguments" member.
		// [CHOSEN, not recovered] S3 bridge: own the native Array separately
		// instead of reproducing the embedded x86 object and virtual bases.
		smart_ptr<as_object>	m_arguments;
		int		m_id;
		movie*		m_parent;
		tu_string	m_name;
		int		m_depth;
		cxform		m_color_transform;
		matrix		m_matrix;
		float		m_ratio;
                Uint16		m_clip_depth;
		bool		m_visible;
		hash<event_id, as_value>	m_event_handlers;

		// MovieClip.setMask. The shipped character keeps the mask clip at
		// +192 and a "mask only" byte at +196 (gameswf_sprite_set_mask); display_list::display
		// draws a masked character as begin_submit_mask / mask / end_submit_mask / character /
		// disable_mask, and never draws a mask-only character on its own.
		smart_ptr<character>	m_mask;
		bool		m_is_mask_only;
		//  the shipped +197: set once a mouse handler is attached (a clip
		// event through set_event_handler, an on... member through the sprite's set_member), read by
		// the sprite mouse pass, never cleared (gameswf_character_ctor zeroes it).
		bool		m_mouse_enabled;

		// [CHOSEN, not recovered] S3 bridge for runtime placement callers;
		// generic characters and sprites retain their definitions through m_def.
		virtual void	hold_definition(character_def* /*def*/) {}

		// Movie teardown (release_script_objects): appends every object or function this character
		// references from script state (event handlers, the mask; overrides add their environment
		// and children) to *held, then clears its own references, so clips of a released movie no
		// longer keep each other alive. Runs no ActionScript.
		// [CHOSEN, not recovered] issue #7 (https://github.com/B2BF/OpenBF2/issues/7).
		virtual void	release_script_values(array<as_value>* held)
		{
			for (hash<event_id, as_value>::iterator it = m_event_handlers.begin(); it != m_event_handlers.end(); ++it)
			{
				hold_script_value(held, it->second);
			}
			m_event_handlers.clear();
			if (m_mask != NULL)
			{
				held->push_back(as_value(m_mask.get_ptr()));
				m_mask = NULL;
			}
		}

		character(movie* parent, int id);

		// Accessors for basic display info.
		virtual character*	cast_to_character() { return this; }	// see movie
		int	get_id() const { return m_id; }
		// SwiffPlayer.dll 0x1006EE40.
		movie*	get_parent() const { return m_parent; }
		void set_parent(movie* parent) { m_parent = parent; }  // for extern movie
		// SwiffPlayer.dll 0x1006EE50 / 0x1006EE60.
		int	get_depth() const { return m_depth; }
		void	set_depth(int d) { m_depth = d; }
		// SwiffPlayer.dll 0x1006EE70 / 0x1006EE80.
		const matrix&	get_matrix() const { return m_matrix; }
		void	set_matrix(const matrix& m)
		{
			assert(m.is_valid());
			m_matrix = m;
		}
		// SwiffPlayer.dll 0x1006EEC0.
		const cxform&	get_cxform() const { return m_color_transform; }
		// SwiffPlayer.dll 0x1007EDB0 copies all eight transform components.
		virtual void	set_cxform(const cxform& cx) { m_color_transform = cx; }
		void	concatenate_cxform(const cxform& cx) { m_color_transform.concatenate(cx); }
		void	concatenate_matrix(const matrix& m) { m_matrix.concatenate(m); }
		// SwiffPlayer.dll 0x1006EED0 / 0x1006EEE0.
		float	get_ratio() const { return m_ratio; }
		void	set_ratio(float f) { m_ratio = f; }
		Uint16	get_clip_depth() const { return m_clip_depth; }
		// SwiffPlayer.dll 0x1006EF00.
		void	set_clip_depth(Uint16 d) { m_clip_depth = d; }
		// SwiffPlayer.dll 0x100A7730 / 0x100A7740 / 0x1006EF20.
		// [CHOSEN, not recovered] S3 bridge: the native mask remains an owning handle.
		character* get_mask() const { return m_mask.get_ptr(); }
		bool is_mask_only() const { return m_is_mask_only; }
		void set_mask_only(bool value) { m_is_mask_only = value; }
		// SwiffPlayer.dll 0x100714A0 leaves a previous mask's flag set.
		// [CHOSEN, not recovered] S3 bridge: retain the native owning handle.
		void set_mask(character* mask)
		{
			m_mask = mask;
			if (mask) mask->m_is_mask_only = true;
		}

		void	set_name(const char* name) { m_name = name; }
		const tu_string&	get_name() const { return m_name; }

		// SwiffPlayer.dll 0x10073490. Copy presentation state to an existing character;
		// its ID, parent, event handlers and script state are retained.
		// [INFERRED] Source-level name; when the source is a sprite, target must be a sprite.
		virtual void	copy_display_state(character* target) const
		{
			target->set_cxform(m_color_transform);
			target->m_name = m_name.c_str();
			target->m_depth = m_depth;
			target->m_matrix = m_matrix;
			target->m_ratio = m_ratio;
			target->m_clip_depth = m_clip_depth;
			target->set_visible(get_visible());
		}

		// For edit_text support (Flash 5).  More correct way
		// is to do "text_character.text = whatever", via
		// set_member().
		// SwiffPlayer.dll 0x1007EDD0 / 0x1007EDE0 (release defaults).
		virtual const char*	get_text_name() const { return ""; }
		virtual void	set_text_value(const char* new_text) { assert(0); }

		// SwiffPlayer.dll 0x1007EDF0.
		virtual matrix	get_world_matrix() const
		// Get our concatenated matrix (all our ancestor transforms, times our matrix).  Maps
		// from our local space into "world" space (i.e. root movie space).
		{
			matrix	m;
			if (m_parent)
			{
				m = m_parent->get_world_matrix();
			}
			m.concatenate(get_matrix());

			return m;
		}

		// SwiffPlayer.dll 0x1007EE80.
		virtual cxform	get_world_cxform() const
		// Get our concatenated color transform (all our ancestor transforms,
		// times our cxform).  Maps from our local space into normal color space.
		{
			cxform	m;
			if (m_parent)
			{
				m = m_parent->get_world_cxform();
			}
			m.concatenate(get_cxform());

			return m;
		}

		// Event handler accessors.
		bool	get_event_handler(event_id id, as_value* result)
		{
			return m_event_handlers.get(id, result);
		}
		void	set_event_handler(event_id id, const as_value& method)
		{
			// SwiffPlayer.dll 0x10077E10; native event IDs use the S2 mapping.
			//  sub_10077E10: a mouse handler marks the character mouse-enabled
			// before it is stored - the eleven ids the shipped body tests.
			switch (id.m_key_code == key::INVALID ? id.m_id : event_id::INVALID)
			{
			case event_id::PRESS:
			case event_id::RELEASE:
			case event_id::RELEASE_OUTSIDE:
			case event_id::ROLL_OVER:
			case event_id::ROLL_OUT:
			case event_id::DRAG_OVER:
			case event_id::DRAG_OUT:
			case event_id::MOUSE_DOWN:
			case event_id::MOUSE_UP:
			case event_id::MOUSE_MOVE:
			case event_id::DOUBLE_PRESS:
				m_mouse_enabled = true;
				break;
			default:
				break;
			}
			m_event_handlers.set(id, method);
		}

		// Movie interfaces.  By default do nothing.  sprite_instance and some others override these.
		// SwiffPlayer.dll 0x1007EEE0 / 0x1007EF10 / 0x1007EF20.
		virtual void	display() {}
		virtual float	get_height() { return 0; }
		virtual float	get_width() { return 0; }

		//  the character's UNSCALED local extent (twips, its own space).
		// Flash's `_width`/`_height` are this times the own matrix scale, and their setters divide by
		// it - absolute, independent of ancestors. See GAMESWF_DIVERGENCES.md #39.
		virtual float	get_local_height() { return 0; }
		virtual float	get_local_width() { return 0; }

		// [CHOSEN, not recovered] S3 bridge for the retained removal/orphan
		// tracer. Sprite member get/set do not consult this flag: retail
		// 0x10080590 / 0x10083A20 have no dead-reference guard.
		virtual void	mark_dead() {}

		virtual movie*	get_root_movie() { return m_parent->get_root_movie(); }
		// SwiffPlayer.dll 0x1007F010 forwards to the parent's environment, or NULL.
		virtual as_environment*	get_action_environment()
		{
			return m_parent != NULL ? m_parent->get_action_environment() : NULL;
		}
		// SwiffPlayer.dll 0x1007EF30 / 0x1007EF40 (release defaults).
		virtual int	get_current_frame() const { assert(0); return 0; }
		virtual bool	has_looped() const { assert(0); return false; }
		// SwiffPlayer.dll 0x1007EF50 / 0x1007EF60 are empty.
		virtual void	restart() { /*assert(0);*/ }
		virtual void	advance(float delta_time) {}	// for buttons and sprites
		// SwiffPlayer.dll 0x1007EF70, primary character table +0x18.
		void apply_goto_frame(int target_frame) override {}
		// Native queued navigation default; distinct from the immediate retail entry.
		virtual void	goto_frame(int target_frame) {}
		// SwiffPlayer.dll 0x1007EF80.
		virtual bool	get_accept_anim_moves() const { return true; }

		// SwiffPlayer.dll 0x1007EF90 leaves the output alone without a parent.
		virtual void	get_drag_state(drag_state* st) { if (m_parent) m_parent->get_drag_state(st); }

		//  sub_1007F030 (character vtable +88, read in full 2026-09-28) -
		// hiding a character RELEASES THE ROOT'S MOUSE CAPTURE when the capture holder is this
		// character or lies anywhere beneath it. The holder took the capture on its press (#51) and
		// drops it only in its own release transition, but a hidden character's mouse pass never
		// runs again - the parent's child loop skips invisible children (0x10080010 tests the +0xBA
		// byte get_visible reads, as #51 does) - so without this a clip that hides what it was
		// pressed in keeps every other character's state machine gated off for good. mainMenu.swf's
		// SelectBox does exactly that: a ColumnList row's onPress selects the row, the box's setValue
		// closes it (m_mcContainer._visible = false) with the pressed row inside, and its click
		// catcher's onPress hides the catcher itself. Owner's report 2026-09-28: after picking an
		// antialiasing level on OPTIONS > VIDEO nothing was clickable, APPLY included.
		//
		//   m_visible = visible; showing stops there
		//   active = the root's get_active_mouse_entity()                  (the root sprite's +0xB4)
		//   walk from active up through the parents, each of which must be a character; reaching
		//   this -> set_active_mouse_entity(NULL)                           (the root sprite's +0xB8)
		//
		// The published root sprite is independent of the caller's tree. [INFERRED]
		// the shipped per-parent test (slot 0 of the +8
		// subobject's table, `mov al, 1` for sprites and buttons alike) is "is a character":
		// cast_to_character here. The BUTTON's `_visible` setter writes the byte directly in both
		// players (sub_100A34E0, gameswf_button.cpp), so hiding a button releases nothing.
		virtual void	set_visible(bool visible)
		{
			// SwiffPlayer.dll 0x1007F030.
			m_visible = visible;
			if (visible)
			{
				return;
			}
			release_mouse_capture_within();
		}

		// The capture walk set_visible ends with (sub_1007F030), also inlined by the display list's
		// remove_display_object (0x100A7A20: 0x100A7A67..0x100A7AAA) and replace_display_object
		// (0x100A8080: 0x100A80B3..0x100A80ED) for the character leaving the list: when the root's
		// active entity is this character or lies beneath it, the capture is dropped. Both list
		// bodies fetch the published root through 0x10071710.
		void	release_mouse_capture_within()
		{
			movie*	owner = get_global_root_movie();
			if (owner == NULL)
			{
				return;
			}
			movie*	active = owner->get_active_mouse_entity();
			if (active == NULL)
			{
				return;
			}
			for (movie* m = active; m != this; )
			{
				character*	c = m->cast_to_character();
				movie*	parent = c != NULL ? c->get_parent() : NULL;
				if (parent == NULL || parent->cast_to_character() == NULL)
				{
					return;
				}
				m = parent;
			}
			// 0x1007F086 / 0x100A7A9F / 0x100A80E2 re-read after callbacks.
			get_global_root_movie()->set_active_mouse_entity(NULL);
		}
		// SwiffPlayer.dll 0x1007EFD0.
		virtual bool	get_visible() const { return m_visible; }

		virtual void	get_mouse_state(int* x, int* y, int* buttons) { get_parent()->get_mouse_state(x, y, buttons); }

		// Utility.
		void	do_mouse_drag();
		// SwiffPlayer.dll character slot +0xF0. Native declaration order differs;
		// all calls use typed dispatch. [INFERRED] source-level name/constness.
		virtual rect get_bound_copy() const = 0;
	};


	// For characters that don't store unusual state in their instances.
	struct generic_character : public character
	{
		// SwiffPlayer.dll 0x1007F130 retains the definition for every instance;
		// 0x10084CA0 releases it before destroying the character base.
		smart_ptr<character_def>	m_def;

		// SwiffPlayer.dll 0x1007F130.
		generic_character(character_def* def, movie* parent, int id)
			:
			character(parent, id),
			m_def(def)
		{
			assert(m_def);
		}

		// [CHOSEN, not recovered] S3 bridge for the native placement API.
		virtual void	hold_definition(character_def* def) { m_def = def; }

		// SwiffPlayer.dll 0x1007F240 forwards to the definition rectangle provider.
		rect get_bound_copy() const override { return m_def->get_bound_copy(); }   // the definition's +0x20

		virtual void	display()
		{
			// SwiffPlayer.dll 0x1007F1D0 delegates to the definition.
			m_def->display(this);	// pass in transform info
		}

		// @@ tulrich: these are used for finding bounds; TODO
		// need to do this using enclose_transformed_rect(),
		// not by scaling the local height/width!

		// SwiffPlayer.dll 0x1007F260 returns pixels after applying world scale.
		virtual float	get_height()
		{
			matrix	m = get_world_matrix();
			return bf2x87::store(bf2x87::mul(bf2x87::mul(
				bf2x87::Real(m_def->get_height_local()), bf2x87::Real(m.m_[1][1])), bf2x87::Real(0.05f)));
		}

		// SwiffPlayer.dll 0x1007F290 returns pixels after applying world scale.
		virtual float	get_width()
		{
			matrix	m = get_world_matrix();
			return bf2x87::store(bf2x87::mul(bf2x87::mul(
				bf2x87::Real(m_def->get_width_local()), bf2x87::Real(m.m_[0][0])), bf2x87::Real(0.05f)));
		}

		//  the definition's local extent, unscaled.
		virtual float	get_local_height() { return m_def->get_height_local(); }
		virtual float	get_local_width() { return m_def->get_width_local(); }

		//  sub_1007F1E0: the world-space point brought into the shape's own
		// space through the inverse of its world matrix, then the definition's local test.
		virtual bool	point_test_world(float x, float y)
		{
			matrix	m = get_world_matrix();
			point	p;
			m.transform_by_inverse(&p, point(x, y));
			return m_def->point_test_local(p.m_x, p.m_y);
		}
	};


	struct bitmap_character_def : public character_def
	{
		virtual gameswf::bitmap_info*	get_bitmap_info() = 0;

		// safe downcast, same pattern as resource::cast_to_font. Lets the
		// exported-bitmap enumeration below tell a bitmap character from any other exported
		// character_def without a static_cast. See GAMESWF_DIVERGENCES.md.
		virtual bitmap_character_def*	cast_to_bitmap_character_def() { return this; }
	};

#if 1
	// Bitmap character
	struct bitmap_character : public bitmap_character_def
	{
		// SwiffPlayer.dll 0x100755A0 / 0x10075570: release the bitmap member,
		// then the definition/ref-counted bases; the latter entry also deletes.
		~bitmap_character() override = default;
		// SwiffPlayer.dll 0x10073940 retains the bitmap after initializing
		// the definition. [CHOSEN, not recovered] S3 bridge: native callers
		// assign the constructor's retail ID separately with set_character_id.
		bitmap_character(bitmap_info* bi)
			:
			m_bitmap_info(bi)
		{
		}

// 		bitmap_character(image::rgb* image)
// 		{
// 			assert(image != 0);

// 			// Create our bitmap info, from our image.
// 			m_bitmap_info = gameswf::render::create_bitmap_info_rgb(image);
// 		}

// 		bitmap_character(image::rgba* image)
// 		{
// 			assert(image != 0);

// 			// Create our bitmap info, from our image.
// 			m_bitmap_info = gameswf::render::create_bitmap_info_rgba(image);
// 		}

		// SwiffPlayer.dll 0x10073A40 returns the held bitmap descriptor.
		gameswf::bitmap_info*	get_bitmap_info()
		{
			return m_bitmap_info.get_ptr();
		}

		// SwiffPlayer.dll 0x100739E0: definition bounds in twips, without a null guard.
		rect	get_bound_copy() const override
		{
			rect bounds;
			bounds.m_x_min = 0.0f;
			bounds.m_x_max = bf2x87::store(bf2x87::mul(
				bf2x87::Real(m_bitmap_info->m_original_width), bf2x87::Real(20.0f)));
			bounds.m_y_min = 0.0f;
			bounds.m_y_max = bf2x87::store(bf2x87::mul(
				bf2x87::Real(m_bitmap_info->m_original_height), bf2x87::Real(20.0f)));
			return bounds;
		}

	private:
		smart_ptr<gameswf::bitmap_info>	m_bitmap_info;
	};

#endif

	// Execute tags include things that control the operation of
	// the movie.  Essentially, these are the events associated
	// with a frame.
	struct execute_tag
	{
		// SwiffPlayer.dll 0x1006EF80 / 0x1006F650 construct only the vptr; 0x1006EFF0
		// is the compiler deleting destructor. Table 0x100DF29C fixes these slots.
		virtual ~execute_tag() {}
		// SwiffPlayer.dll 0x1006EF90 / 0x1006EFA0 leave the movie untouched.
		virtual void	execute(movie* m) {}
		virtual void	execute_state(movie* m) {}
		virtual void	execute_state_reverse(movie* m, int frame) { execute_state(m); }
		// SwiffPlayer.dll 0x1006EFC0 / 0x1006EFD0 return false.
		virtual bool	is_remove_tag() const { return false; }
		virtual bool	is_action_tag() const { return false; }
		virtual uint32	get_depth_id_of_replace_or_add_tag() const { return static_cast<uint32>(-1); }

		//  The shipped execute_tag's third execution slot (vtable +12,
		// base 0x1006EFB0): what a tag does when a goto replays the TARGET frame's playlist after
		// the timeline-state diff has already placed, moved and removed everything. The base runs
		// execute(), so a do_action (which leaves the slot at the base) queues its actions, while
		// place_object_2 (0x10075760) and remove_object_2 (0x1006F8E0) override it with nothing.
		// The other three are the reads the shipped state builder (sub_100832F0) makes once it has
		// identified a tag: is_place_tag (vtable +24), then the place tag's depth (+70) and kind
		// (+76, place_object_2::place_type), or the remove tag's depth (+4).
		virtual void	execute_goto(movie* m) { execute(m); }
		// SwiffPlayer.dll 0x1006EFE0 returns false.
		virtual bool	is_place_tag() const { return false; }
		virtual int	get_tag_depth() const { return -1; }
		virtual int	get_place_type() const { return -1; }
	};


	//
	// Loader callbacks.
	//
	
	// Register a loader function for a certain tag type.  Most
	// standard tags are handled within gameswf.  Host apps might want
	// to call this in order to handle special tag types.
	typedef void (*loader_function)(stream* input, int tag_type, movie_definition_sub* m);
	void	register_tag_loader(int tag_type, loader_function lf);
	
	
	// Tag loader functions.
	void	null_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	set_background_color_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	jpeg_tables_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_jpeg_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_jpeg2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_jpeg3_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_shape_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_shape_morph_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_font_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_font_info_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_text_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_edit_text_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	place_object_2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_bits_lossless_2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	sprite_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	end_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	remove_object_2_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	do_action_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	button_character_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	frame_label_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	export_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	import_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	define_sound_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	start_sound_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	button_sound_loader(stream* in, int tag_type, movie_definition_sub* m);
	void	do_init_action_loader(stream* in, int tag_type, movie_definition_sub* m);
	// sound_stream_loader();	// head, head2, block


}	// end namespace gameswf


#endif // GAMESWF_IMPL_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
