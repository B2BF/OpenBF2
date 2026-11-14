// gameswf_impl.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some implementation for SWF player.

// Useful links:
//
// http://sswf.sourceforge.net/SWFalexref.html
// http://www.openswf.org

// @@ Need to break this file into pieces


#include <string.h>
#include <typeinfo>
#include <float.h>

#include "base/tu_file.h"
#include "../../Core/MsvcCrtCompat.h"   // [CHOSEN, not recovered] _snprintf_s/_TRUNCATE outside MSVC
#include "../../Platform/X87Compat.h"
#include "base/utility.h"
#include "gameswf_action.h"
#include "gameswf_button.h"
#include "gameswf_impl.h"
#include "gameswf_font.h"
#include "gameswf_fontlib.h"
#include "gameswf_log.h"
#include "gameswf_profile.h"
#include "gameswf_morph2.h"
#include "gameswf_render.h"
#include "gameswf_shape.h"
#include "gameswf_stream.h"
#include "gameswf_styles.h"
#include "gameswf_dlist.h"
#include "gameswf_timers.h"
#include "base/image.h"
#include "base/jpeg.h"
#include "base/tu_timer.h"




namespace gameswf
{
	// SwiffPlayer.dll 0x100F8CAC; source-level name [INFERRED].
	static smart_ptr<movie> s_global_root_movie;

	movie* get_global_root_movie()
	{
		// SwiffPlayer.dll 0x10071710: borrow the published sprite, without lookup.
		return s_global_root_movie.get_ptr();
	}

	void set_global_root_movie(movie* root)
	{
		// SwiffPlayer.dll 0x100738A0: identity check, release, store, retain.
		s_global_root_movie = root;
	}

	// SwiffPlayer.dll 0x100F8B78; source-level name [INFERRED].
	static Uint32 s_character_live_count = 0;

	character::character(movie* parent, int id)
		:
		m_arguments(create_array_object()),
		m_id(id),
		m_parent(parent),
		m_depth(-1),
		m_ratio(0.0f),
		m_clip_depth(0),
		m_visible(true),
		m_is_mask_only(false),
		m_mouse_enabled(false)
	{
		assert((parent == NULL && m_id == -1)
		       || (parent != NULL && m_id >= 0));
		// SwiffPlayer.dll 0x1007EC60 increments after initializing character
		// members. Common/embedded-Array construction retains the native S3 bridges.
		++s_character_live_count;
	}

	character::~character()
	{
		// SwiffPlayer.dll 0x1007F0A0 and its inline copy in 0x10089D50
		// decrement before event-handler, name and embedded-Array destruction.
		// Native mask/Array ownership and common lifetime remain S3 bridges.
		--s_character_live_count;
	}

	bool	s_verbose_action = false;
	bool	s_verbose_parse = false;
	// SwiffPlayer.dll 0x1006F120 / 0x1006F130; source-level names [INFERRED].
	bool s_verbose_loader = false;
	bool s_verbose_gc = false;
	void set_verbose_loader(bool verbose) { s_verbose_loader = verbose; }
	void set_verbose_gc(bool verbose) { s_verbose_gc = verbose; }
	// SwiffPlayer.dll 0x1006F110 writes the mouse-event verbosity switch.
	// No retail caller is known. The event dispatcher reads it at 0x10081650.
	static bool s_verbose_mouse_events = false;
	void set_verbose_mouse_events(bool verbose) { s_verbose_mouse_events = verbose; }
	// SwiffPlayer.dll 0x1006F140; [INFERRED] source-level names.
	bool s_profiling = false;
	void set_profiling(bool enabled) { s_profiling = enabled; }

	// [INFERRED] source-level type and field names. The registry borrows the
	// static records; destroying a record releases its name, without unregistering.
	static array<profile_record*> s_profile_records;
	profile_record::profile_record(const char* name)
		: m_count(0), m_ticks(0), m_name(name)
	{
		s_profile_records.push_back(this);
	}

	const array<profile_record*>& get_profile_records() { return s_profile_records; }

#ifndef NDEBUG
	bool	s_verbose_debug = true;
#else
	bool	s_verbose_debug = false;
#endif


	void	set_verbose_action(bool verbose)
	// Enable/disable log messages re: actions.
	{
		// SwiffPlayer.dll 0x1006F0F0.
		s_verbose_action = verbose;
	}


	void	set_verbose_parse(bool verbose)
	// Enable/disable log messages re: parsing the movie.
	{
		// SwiffPlayer.dll 0x1006F100.
		s_verbose_parse = verbose;
	}


	// SwiffPlayer.dll 0x1006F0E0; [INFERRED] native name of the placement/removal gate.
	void set_verbose_debug(bool verbose) { s_verbose_debug = verbose; }

	static bool	s_use_cache_files = true;

	void	set_use_cache_files(bool use_cache)
	// Enable/disable attempts to read cache files when loading
	// movies.
	{
		// SwiffPlayer.dll 0x1006F150 stores the initially true byte; name [INFERRED].
		s_use_cache_files = use_cache;
	}


	// Keep a table of loader functions for the different tag types.
	static hash<int, loader_function>	s_tag_loaders;

	// SwiffPlayer.dll 0x10073220.
	void	register_tag_loader(int tag_type, loader_function lf)
	// Associate the specified tag type with the given tag loader
	// function.
	{
		assert(s_tag_loaders.get(tag_type, NULL) == false);
		assert(lf != NULL);

		s_tag_loaders.add(tag_type, lf);
	}


	//
	// file_opener callback stuff
	//

	static file_opener_callback	s_opener_function = NULL;

	void	register_file_opener_callback(file_opener_callback opener)
	// Host calls this to register a function for opening files,
	// for loading movies.
	{
		// SwiffPlayer.dll 0x1006F160; 0x1006F386 / 0x1007D3C6 use this opener.
		s_opener_function = opener;
	}

	//
	// some utility stuff
	//


	//  execute_actions (index walk over the live list) is gone: its
	// only caller, sprite_instance::do_actions, now drains the list the way the shipped body does.


	// SwiffPlayer.dll 0x1006F170.
	static void	dump_tag_bytes(stream* in)
	// Log the contents of the current tag, in hex.
	{
		static const int	ROW_BYTES = 16;
		char	row_buf[ROW_BYTES];
		int	row_count = 0;

		while(in->get_position() < in->get_tag_end_position())
		{
			int	c = in->read_u8();
			log_msg("%02X", c);

			if (c < 32) c = '.';
			if (c > 127) c = '.';
			row_buf[row_count] = c;
			
			row_count++;
			if (row_count >= ROW_BYTES)
			{
				log_msg("    ");
				for (int i = 0; i < ROW_BYTES; i++)
				{
					log_msg("%c", row_buf[i]);
				}

				log_msg("\n");
				row_count = 0;
			}
			else
			{
				log_msg(" ");
			}
		}

		if (row_count > 0)
		{
			log_msg("\n");
		}
	}


	// SwiffPlayer.dll 0x1007F2C0. [CHOSEN, not recovered] S3 bridge:
	// the native factory hands its borrowed raw result to the caller's smart_ptr.
	character*	character_def::create_character_instance(movie* parent, int id)
	// Default.  Make a generic_character.
	{
		return new generic_character(this, parent, id);
	}


	//
	// ref_counted
	//


	// SwiffPlayer.dll 0x1006F240.
	ref_counted::ref_counted()
		:
		m_ref_count(0),
		m_weak_proxy(0)
	{
	}

	// SwiffPlayer.dll 0x1006F260; deleting entry 0x10071570.
	ref_counted::~ref_counted()
	{
		assert(m_ref_count == 0);

		if (m_weak_proxy)
		{
			m_weak_proxy->notify_object_died();
			m_weak_proxy->drop_ref();
		}
	}

	// SwiffPlayer.dll 0x1006F290.
	void	ref_counted::add_ref() const
	{
		assert(m_ref_count >= 0);
		m_ref_count++;
	}

	// SwiffPlayer.dll 0x1006F2A0; character secondary entry 0x1007EFF0.
	void	ref_counted::drop_ref() const
	{
		assert(m_ref_count > 0);
		m_ref_count--;
		if (m_ref_count <= 0)
		{
			// Delete me!
			delete this;
		}
	}

	// SwiffPlayer.dll 0x1006F2C0: allocate and retain one proxy on first request.
	weak_proxy* ref_counted::get_weak_proxy() const
	{
		assert(m_ref_count > 0);	// By rights, somebody should be holding a ref to us.

		if (m_weak_proxy == NULL)
		{
			m_weak_proxy = new weak_proxy;
			m_weak_proxy->add_ref();
		}

		return m_weak_proxy;
	}


	//
	// character
	//


	void	character::do_mouse_drag()
	// Implement mouse-dragging for this movie.
	{
		// SwiffPlayer.dll 0x10073250.
		drag_state st;
		get_drag_state(&st);
		if (this == st.m_character)
		{
			int x, y, buttons;
			get_root_movie()->get_mouse_state(&x, &y, &buttons);
			point world_mouse(float(x) * 20.0f, float(y) * 20.0f);
			matrix parent_world;
			if (st.m_lock_center)
			{
				if (st.m_bound)
				{
					world_mouse.m_x = fclamp(world_mouse.m_x, st.m_bound_x0, st.m_bound_x1);
					world_mouse.m_y = fclamp(world_mouse.m_y, st.m_bound_y0, st.m_bound_y1);
				}
				matrix world = get_world_matrix();
				point local_mouse;
				world.transform_by_inverse(&local_mouse, world_mouse);
				if (m_parent) parent_world = m_parent->get_world_matrix();
			}
			else
			{
				matrix world = get_world_matrix();
				point local_mouse;
				world.transform_by_inverse(&local_mouse, world_mouse);
				if (m_parent) parent_world = m_parent->get_world_matrix();
				world_mouse.m_x -= st.m_offset_x;
				world_mouse.m_y -= st.m_offset_y;
				if (st.m_bound)
				{
					world_mouse.m_x = fclamp(world_mouse.m_x, st.m_bound_x0, st.m_bound_x1);
					world_mouse.m_y = fclamp(world_mouse.m_y, st.m_bound_y0, st.m_bound_y1);
				}
			}
			point parent_mouse;
			parent_world.transform_by_inverse(&parent_mouse, world_mouse);
			matrix local = get_matrix();
			local.m_[0][2] = parent_mouse.m_x;
			local.m_[1][2] = parent_mouse.m_y;
			set_matrix(local);
		}
	}


	//
	// Helper for movie_def_impl
	//

	struct import_info
	{
		tu_string	m_source_url;
		int	        m_character_id;
		tu_string	m_symbol;

		import_info()
			:
			m_character_id(-1)
		{
		}

		import_info(const char* source, int id, const char* symbol)
			:
			m_source_url(source),
			m_character_id(id),
			m_symbol(symbol)
		{
		}
	};


	//
	// movie_def_impl
	//
	// This class holds the immutable definition of a movie's
	// contents.  It cannot be played directly, and does not hold
	// current state; for that you need to call create_instance()
	// to get a movie_instance.
	//


	struct movie_def_impl : public movie_definition_sub
	{
		hash<int, smart_ptr<character_def> >	m_characters;
		hash<int, smart_ptr<font> >	 m_fonts;
		hash<int, smart_ptr<bitmap_character_def> >	m_bitmap_characters;
		hash<int, smart_ptr<sound_sample> >	m_sound_samples;
		array<array<execute_tag*> >	   m_playlist;	// A list of movie control events for each frame.
		array<execute_tag*>	   m_init_action_list;
		// [CHOSEN, not recovered] S3 bridge: retail stores the pending bit
		// in each action_buffer; the native do_action wrapper owns that buffer.
		array<bool> m_init_action_pending;
		stringi_hash<int>	           m_named_frames;	// 0-based frame #'s
		stringi_hash<smart_ptr<resource> > m_exports;
		hash<int, tu_string> m_export_names;

		// Items we import.
		array<import_info>	m_imports;

		// Movies we import from; hold a ref on these, to keep them alive
		array<smart_ptr<movie_definition> >	m_import_source_movies;

		// Bitmaps used in this movie; collected in one place to make
		// it possible for the host to manage them as textures.
		array<smart_ptr<bitmap_info> >	m_bitmap_list;

		create_bitmaps_flag	m_create_bitmaps;
		create_font_shapes_flag	m_create_font_shapes;

		rect	m_frame_size;
		float	m_frame_rate;
		int	m_frame_count;
		int	m_version;
		int	m_loading_frame;
		// [UNRESOLVED] Retail +0x88 starts at 1000 (0x1007807D), separately
		// from the file header's length. [INFERRED] word name and unsigned type.
		Uint32 m_retail_word_88;
		uint32	m_file_length;

		jpeg::input*	m_jpeg_in;


		// SwiffPlayer.dll 0x10077FB0; native containers/virtual bases supply
		// the empty inherited and owned storage without retail x86 offsets.
		movie_def_impl(create_bitmaps_flag cbf, create_font_shapes_flag cfs)
			:
			m_create_bitmaps(cbf),
			m_create_font_shapes(cfs),
			m_frame_rate(30.0f),
			m_frame_count(0),
			m_version(0),
			m_loading_frame(0),
			m_retail_word_88(1000),
			m_jpeg_in(0)
		{
			// SwiffPlayer.dll 0x10077FB0 zeroes +0x08..+0x34. A movie definition has no definition ID
			// (it is not a character_def); see sprite_definition_of for the reads that land there.
		}

		~movie_def_impl()
		{
			// SwiffPlayer.dll 0x1007CE20 (deleting entry 0x1007CDE0)
			// deletes init actions before frame tags, rechecking the init count.
			// [CHOSEN, not recovered] S3 bridge: native execute_tag wrappers own the buffers.
			{for (int i = 0; i < m_init_action_list.size(); i++)
			{
				delete m_init_action_list[i];
			}}

			// Release our playlist data.
			{for (int i = 0, n = m_playlist.size(); i < n; i++)
			{
				for (int j = 0, m = m_playlist[i].size(); j < m; j++)
				{
					delete m_playlist[i][j];
				}
			}}

			assert(m_jpeg_in == NULL);	// It's supposed to be cleaned up in read()
		}


		movie_interface*	create_instance();

		// ...
		// SwiffPlayer.dll 0x100780A0.
		int	get_frame_count() const { return m_frame_count; }
		// SwiffPlayer.dll 0x100780B0.
		float	get_frame_rate() const { return m_frame_rate; }
		// SwiffPlayer.dll 0x100780C0.
		float	get_width_pixels() const
		{
			return static_cast<float>(ceil(bf2x87::storeDouble(bf2x87::mul(
				bf2x87::sub(bf2x87::Real(m_frame_size.m_x_max), bf2x87::Real(m_frame_size.m_x_min)),
				bf2x87::Real(0.05f)))));
		}
		// SwiffPlayer.dll 0x100780E0.
		float	get_height_pixels() const
		{
			return static_cast<float>(ceil(bf2x87::storeDouble(bf2x87::mul(
				bf2x87::sub(bf2x87::Real(m_frame_size.m_y_max), bf2x87::Real(m_frame_size.m_y_min)),
				bf2x87::Real(0.05f)))));
		}

		// SwiffPlayer.dll 0x10078100.
		virtual int	get_version() const { return m_version; }

		// SwiffPlayer.dll 0x10078110.
		virtual int	get_loading_frame() const { return m_loading_frame; }

		uint32	get_file_bytes() const { return m_file_length; }

		// SwiffPlayer.dll 0x10078120.
		virtual create_bitmaps_flag	get_create_bitmaps() const
		// Returns DO_CREATE_BITMAPS if we're supposed to
		// initialize our bitmap infos, or DO_NOT_INIT_BITMAPS
		// if we're supposed to create blank placeholder
		// bitmaps (to be init'd later explicitly by the host
		// program).
		{
			return m_create_bitmaps;
		}

		// SwiffPlayer.dll 0x10078130.
		virtual create_font_shapes_flag	get_create_font_shapes() const
		// Returns DO_LOAD_FONT_SHAPES if we're supposed to
		// initialize our font shape info, or
		// DO_NOT_LOAD_FONT_SHAPES if we're supposed to not
		// create any (vector) font glyph shapes, and instead
		// rely on precached textured fonts glyphs.
		{
			return m_create_font_shapes;
		}

		// SwiffPlayer.dll 0x100781B0.
		virtual void	add_bitmap_info(bitmap_info* bi)
		// All bitmap_info's used by this movie should be
		// registered with this API.
		{
			m_bitmap_list.push_back(bi);
		}


		// walk m_exports, keeping the entries whose resource is a bitmap
		// character. The NAME is the ExportAssets symbol, which for BF2's menu bitmaps is the
		// external image path. See gameswf.h and GAMESWF_DIVERGENCES.md.
		virtual int	get_exported_bitmaps(int max, tu_string* names,
						     gameswf::bitmap_info** infos) const
		{
			int	n = 0;
			for (stringi_hash<smart_ptr<resource> >::const_iterator it = m_exports.begin();
			     it != m_exports.end() && n < max; ++it)
			{
				resource*	res = it.get_value().get_ptr();
				if (res == NULL) continue;
				character_def*	cd = res->cast_to_character_def();
				if (cd == NULL) continue;
				bitmap_character_def*	bcd = cd->cast_to_bitmap_character_def();
				if (bcd == NULL) continue;
				// get_key() returns the tu_stringi BY VALUE - copy it, never keep its c_str().
				names[n] = it.get_key();
				infos[n] = bcd->get_bitmap_info();
				n++;
			}
			return n;
		}

		// SwiffPlayer.dll 0x10078260, definition vtable +0x3C.
		virtual bool get_export_name(int id, tu_string* out)
		{
			return m_export_names.get(id, out);
		}

		// [CHOSEN, not recovered] S3 bridge: legacy pointer-to-ID query,
		// retained until the swf-text-font-id menu validation gate.
		virtual bool	get_exported_font_name(const font* f, tu_string* out)
		{
			for (hash<int, smart_ptr<font> >::const_iterator it = m_fonts.begin();
			     it != m_fonts.end(); ++it)
			{
				if (it->second.get_ptr() == f) return get_export_name(it->first, out);
			}
			return false;
		}

		// SwiffPlayer.dll 0x10078170.
		virtual int	get_bitmap_info_count() const { return m_bitmap_list.size(); }
		// SwiffPlayer.dll 0x10078180.
		virtual bitmap_info*	get_bitmap_info(int i) const
		{
			return m_bitmap_list[i].get_ptr();
		}

		// SwiffPlayer.dll 0x10079FF0 stores both symbol-to-resource and ID-to-name.
		virtual void	export_resource(int id, const tu_string& symbol, resource* res)
		// Expose one of our resources under the given symbol,
		// for export.  Other movies can import it.
		{
			// SWF sometimes exports the same thing more than once!
			m_exports.set(symbol, res);
			m_export_names.set(id, symbol);
		}

		// SwiffPlayer.dll 0x10079D50.
		virtual smart_ptr<resource>	get_exported_resource(const tu_string& symbol)
		// Get the named exported resource, if we expose it.
		// Otherwise return NULL.
		{
			smart_ptr<resource>	res;
			m_exports.get(symbol, &res);
			return res;
		}

		virtual void	release_registered_classes()
		// [CHOSEN, not recovered] issue #7: Object.registerClass binds through the exports only.
		{
			for (stringi_hash<smart_ptr<resource> >::iterator it = m_exports.begin(); it != m_exports.end(); ++it)
			{
				character_def*	cdef = it->second != NULL ? it->second->cast_to_character_def() : NULL;
				if (cdef != NULL && cdef->m_registered_class != NULL)
				{
					cdef->m_registered_class->drop_ref();
					cdef->m_registered_class = NULL;
				}
			}
			// [CHOSEN, not recovered] issue #7: the cached definition must
			// initialize the next movie's script classes after action_clear().
			for (int i = 0; i < m_init_action_pending.size(); ++i) m_init_action_pending[i] = true;
		}

		// SwiffPlayer.dll 0x100794C0.
		virtual void	add_import(const char* source_url, int id, const char* symbol)
		// Adds an entry to a table of resources that need to
		// be imported from other movies.  Client code must
		// call resolve_import() later, when the source movie
		// has been loaded, so that the actual resource can be
		// used.
		{
			assert(in_import_table(id) == false);

			m_imports.push_back(import_info(source_url, id, symbol));
		}

		bool	in_import_table(int character_id)
		// Debug helper; returns true if the given
		// character_id is listed in the import table.
		{
			for (int i = 0, n = m_imports.size(); i < n; i++)
			{
				if (m_imports[i].m_character_id == character_id)
				{
					return true;
				}
			}
			return false;
		}

		// SwiffPlayer.dll 0x1007A480: snapshot the count, visit once per URL,
		// and record the URL only after its callback returns.
		virtual void	visit_imported_movies(import_visitor* visitor)
		// Calls back the visitor for each movie that we
		// import symbols from.
		{
			stringi_hash<bool>	visited;	// ugh!

			for (int i = 0, n = m_imports.size(); i < n; i++)
			{
				import_info&	inf = m_imports[i];
				if (visited.find(inf.m_source_url) == visited.end())
				{
					// Call back the visitor.
					visitor->visit(inf.m_source_url.c_str());
					visited.set(inf.m_source_url, true);
				}
			}
		}

		// SwiffPlayer.dll 0x10079560 removes matching imports even on refusal.
		virtual void	resolve_import(const char* source_url, movie_definition* source_movie)
		// Grabs the stuff we want from the source movie.
		{
			// @@ should be safe, but how can we verify
			// it?  Compare a member function pointer, or
			// something?
			movie_def_impl*	def_impl = static_cast<movie_def_impl*>(source_movie);
			movie_definition_sub*	def = static_cast<movie_definition_sub*>(def_impl);

			// Iterate in reverse, since we remove stuff along the way.
			for (int i = m_imports.size() - 1; i >= 0; i--)
			{
				const import_info&	inf = m_imports[i];
				if (inf.m_source_url == source_url)
				{
					// Do the import.
					smart_ptr<resource> res = def->get_exported_resource(inf.m_symbol);
					bool	 imported = true;

					if (res == NULL)
					{
						log_error("import error: resource '%s' is not exported from movie '%s'\n",
							  inf.m_symbol.c_str(), source_url);
					}
					else if (font* f = res->cast_to_font())
					{
						// Add this shared font to our fonts.
						add_font(inf.m_character_id, f);
						imported = true;
					}
					else if (character_def* ch = res->cast_to_character_def())
					{
						// Add this character to our characters.
						add_character(inf.m_character_id, ch);
						imported = true;
					}
					else
					{
						log_error("import error: resource '%s' from movie '%s' has unknown type\n",
							  inf.m_symbol.c_str(), source_url);
					}

					if (imported)
					{
						m_imports.remove(i);

						// Hold a ref, to keep this source movie_definition alive.
						m_import_source_movies.push_back(source_movie);
					}
				}
			}
		}

		// SwiffPlayer.dll 0x100782A0.
		void	add_character(int character_id, character_def* c)
		{
			assert(c);
			m_characters.add(character_id, c);
		}

		// SwiffPlayer.dll 0x100784F0, definition vtable +0x40.
		character_def*	get_character_def(int character_id)
		{
#ifndef NDEBUG
			// make sure character_id is resolved
			if (in_import_table(character_id))
			{
				log_error("get_character_def(): character_id %d is still waiting to be imported\n",
					  character_id);
			}
#endif // not NDEBUG

			smart_ptr<character_def>	ch;
			m_characters.get(character_id, &ch);
			assert(ch == NULL || ch->get_ref_count() > 1);
			return ch.get_ptr();
		}

		// SwiffPlayer.dll 0x10079F20.
		bool	get_labeled_frame(const char* label, int* frame_number)
		// Returns 0-based frame #
		{
			return m_named_frames.get(label, frame_number);
		}

		// SwiffPlayer.dll 0x100785E0.
		void	add_font(int font_id, font* f)
		{
			assert(f);
			m_fonts.add(font_id, f);
		}

		// SwiffPlayer.dll 0x10078830.
		font*	get_font(int font_id)
		{
#ifndef NDEBUG
			// make sure font_id is resolved
			if (in_import_table(font_id))
			{
				log_error("get_font(): font_id %d is still waiting to be imported\n",
					  font_id);
			}
#endif // not NDEBUG

			smart_ptr<font>	f;
			m_fonts.get(font_id, &f);
			assert(f == NULL || f->get_ref_count() > 1);
			return f.get_ptr();
		}

		// SwiffPlayer.dll 0x10078920.
		bitmap_character_def*	get_bitmap_character(int character_id)
		{
			smart_ptr<bitmap_character_def>	ch;
			m_bitmap_characters.get(character_id, &ch);
			assert(ch == NULL || ch->get_ref_count() > 1);
			return ch.get_ptr();
		}

		// SwiffPlayer.dll 0x10078A10.
		void	add_bitmap_character(int character_id, bitmap_character_def* ch)
		{
			assert(ch);
			m_bitmap_characters.add(character_id, ch);

			add_bitmap_info(ch->get_bitmap_info());
		}

		// SwiffPlayer.dll 0x10078C20.
		sound_sample*	get_sound_sample(int character_id)
		{
			smart_ptr<sound_sample>	ch;
			m_sound_samples.get(character_id, &ch);
			assert(ch == NULL || ch->get_ref_count() > 1);
			return ch.get_ptr();
		}

		// SwiffPlayer.dll 0x10078D50.
		virtual void	add_sound_sample(int character_id, sound_sample* sam)
		{
			assert(sam);
			m_sound_samples.add(character_id, sam);
		}

		// SwiffPlayer.dll 0x10078F50.
		void	add_execute_tag(execute_tag* e)
		{
			assert(e);
			m_playlist[m_loading_frame].push_back(e);
		}

		// SwiffPlayer.dll 0x10078F90 appends to one definition-wide list.
		void	add_init_action(execute_tag* e)
		{
			assert(e);
			m_init_action_list.push_back(e);
			m_init_action_pending.push_back(true);
		}

		// SwiffPlayer.dll 0x10079F90.
		void	add_frame_name(const char* name)
		// Labels the frame currently being loaded with the
		// given name.  A copy of the name string is made and
		// kept in this object.
		{
			assert(m_loading_frame >= 0 && m_loading_frame < m_frame_count);

			tu_string	n = name;
			assert(m_named_frames.get(n, NULL) == false);	// frame should not already have a name (?)
			m_named_frames.add(n, m_loading_frame);	// stores 0-based frame #
		}

		// SwiffPlayer.dll 0x10078140.
		void	set_jpeg_loader(jpeg::input* j_in)
		// Set an input object for later loading DefineBits
		// images (JPEG images without the table info).
		{
			assert(m_jpeg_in == NULL);
			m_jpeg_in = j_in;
		}

		// SwiffPlayer.dll 0x10078150.
		jpeg::input*	get_jpeg_loader()
		// Get the jpeg input loader, to load a DefineBits
		// image (one without table info).
		{
			return m_jpeg_in;
		}


		// SwiffPlayer.dll 0x10078190.
		virtual const array<execute_tag*>&	get_playlist(int frame_number) { return m_playlist[frame_number]; }

		// SwiffPlayer.dll 0x10078160 returns the entire init-action list.
		virtual const array<execute_tag*>*	get_init_actions(int frame_number)
		{
			return &m_init_action_list;
		}

		// SwiffPlayer.dll 0x1007AA80. Font-name aliases are built after all tags.
		void	read(tu_file* in)
		// Read a .SWF movie.
		{
			Uint32	file_start_pos = in->get_position();
			Uint32	header = in->read_le32();
			m_file_length = in->read_le32();
			Uint32	file_end_pos = file_start_pos + m_file_length;

			m_version = (header >> 24) & 255;
			if ((header & 0x0FFFFFF) != 0x00535746
			    && (header & 0x0FFFFFF) != 0x00535743)
			{
				// ERROR
				log_error("gameswf::movie_def_impl::read() -- file does not start with a SWF header!\n");
				return;
			}
			bool	compressed = (header & 255) == 'C';

			IF_VERBOSE_PARSE(log_msg("version = %d, file_length = %d\n", m_version, m_file_length));

			if (compressed)
			{
				log_error("movie_def_impl::read(): unable to read zipped SWF data; TU_CONFIG_LINK_TO_ZLIB is 0\n");
				return;

			}

			stream	str(in);

			m_frame_size.read(&str);
			m_frame_rate = str.read_u16() / 256.0f;
			m_frame_count = str.read_u16();

			m_playlist.resize(m_frame_count);

			IF_VERBOSE_PARSE(m_frame_size.print());
			IF_VERBOSE_PARSE(log_msg("frame rate = %f, frames = %d\n", m_frame_rate, m_frame_count));

			while ((Uint32) str.get_position() < file_end_pos)
			{
				int	tag_type = str.open_tag();

				loader_function	lf = NULL;
				IF_VERBOSE_PARSE(log_msg("tag_type = %d\n", tag_type));
				if (tag_type == 1)
				{
					// show frame tag -- advance to the next frame.
					m_loading_frame++;
				}
				else if (s_tag_loaders.get(tag_type, &lf))
				{
					// call the tag loader.  The tag loader should add
					// characters or tags to the movie data structure.
					(*lf)(&str, tag_type, this);

				} else {
					// no tag loader for this tag type.
					IF_VERBOSE_PARSE(log_msg("*** no tag loader for type %d\n", tag_type));
					IF_VERBOSE_PARSE(dump_tag_bytes(&str));
				}

				str.close_tag();

				if (tag_type == 0)
				{
					if ((unsigned int) str.get_position() != file_end_pos)
					{
						// Safety break, so we don't read past the end of the
						// movie.
						log_msg("warning: hit stream-end tag, but not at the "
							"end of the file yet; stopping for safety\n");
						break;
					}
				}
			}

			stringi_hash<int> exported_font_ids;
			for (hash<int, smart_ptr<font> >::iterator it = m_fonts.begin();
			     it != m_fonts.end(); ++it)
			{
				tu_string name;
				if (get_export_name(it->first, &name))
				{
					exported_font_ids.set(it->second->get_name(), it->first);
				}
			}
			for (hash<int, smart_ptr<font> >::iterator it = m_fonts.begin();
			     it != m_fonts.end(); ++it)
			{
				tu_string name;
				if (!get_export_name(it->first, &name))
				{
					int exported_id;
					if (exported_font_ids.get(it->second->get_name(), &exported_id))
					{
						get_export_name(exported_id, &name);
						m_export_names.set(it->first, name);
					}
				}
			}

			if (m_jpeg_in)
			{
				delete m_jpeg_in;
				m_jpeg_in = NULL;
			}

		}


		// SwiffPlayer.dll 0x100798E0 appends owned fonts in dictionary order.
		void	get_owned_fonts(array<font*>* fonts)
		// Fill up *fonts with fonts that we own.
		{
			assert(fonts);

			for (hash<int, smart_ptr<font> >::iterator it = m_fonts.begin();
			     it != m_fonts.end();
			     ++it)
			{
				font*	f = it->second.get_ptr();
				if (f->get_owning_movie() == this)
				{
					fonts->push_back(f);
				}
			}
		}


		// SwiffPlayer.dll 0x100797E0.
		void	generate_font_bitmaps()
		// Generate bitmaps for our fonts, if necessary.
		{
			// Collect list of fonts.
			array<font*>	fonts;
			get_owned_fonts(&fonts);
			fontlib::generate_font_bitmaps(fonts, this);
		}


		// Increment this when the cache data format changes.
		// SwiffPlayer.dll 0x100799BA and 0x10079C0F use version 3.
		#define CACHE_FILE_VERSION 3


		// SwiffPlayer.dll 0x10079990.
		void	output_cached_data(tu_file* out, const cache_options& options)
		// Dump our cached data into the given stream.
		{
			// Write a little header.
			char	header[5];
			strcpy(header, "gscX");
			header[3] = CACHE_FILE_VERSION;
			compiler_assert(CACHE_FILE_VERSION < 256);

			out->write_bytes(header, 4);

			// Write font data.
			array<font*>	fonts;
			get_owned_fonts(&fonts);
			fontlib::output_cached_data(out, fonts, this, options);

			// Write character data.
			{for (hash<int, smart_ptr<character_def> >::iterator it = m_characters.begin();
			     it != m_characters.end();
			     ++it)
			{
				out->write_le16(it->first);
				it->second->output_cached_data(out, options);
			}}

			out->write_le16((Sint16) -1);	// end of characters marker
		}


		// SwiffPlayer.dll 0x10079BD0.
		void	input_cached_data(tu_file* in)
		// Read in cached data and use it to prime our loaded characters.
		{
			// Read the header & check version.
			unsigned char	header[4];
			in->read_bytes(header, 4);
			if (header[0] != 'g' || header[1] != 's' || header[2] != 'c')
			{
				log_error("cache file does not have the correct format; skipping\n");
				return;
			}
			else if (header[3] != CACHE_FILE_VERSION)
			{
				log_error(
					"cached data is version %d, but we require version %d; skipping\n",
					int(header[3]), CACHE_FILE_VERSION);
				return;
			}

			// Read the cached font data.
			array<font*>	fonts;
			get_owned_fonts(&fonts);
			fontlib::input_cached_data(in, fonts, this);

			// Read the cached character data.
			for (;;)
			{
				if (in->get_error() != TU_FILE_NO_ERROR)
				{
					log_error("error reading cache file (characters); skipping\n");
					return;
				}
				if (in->get_eof())
				{
					log_error("unexpected eof reading cache file (characters); skipping\n");
					return;
				}

				Sint16	id = in->read_le16();
				if (id == (Sint16) -1) { break; }	// done

				smart_ptr<character_def> ch;
				m_characters.get(id, &ch);
				if (ch != NULL)
				{
					ch->input_cached_data(in);
				}
				else
				{
					log_error("sync error in cache file (reading characters)!  "
						  "Skipping rest of cache data.\n");
					return;
				}
			}
		}

	};


	//
	// movie_root
	//
	// Global, shared root state for a movie and all its characters.
	//

	// SwiffPlayer.dll 0x10073560 / 0x10075370. [CHOSEN, not recovered]
	// S3 bridge: the native root owns its movie per tree, in place of the retail
	// global. Userdata and initialized drag offsets
	// remain native extensions; this class does not reproduce the x86 layout.
	struct movie_root : public movie_interface
	{
		smart_ptr<movie_def_impl>	m_def;
		smart_ptr<movie>	m_movie;
		int			m_viewport_x0, m_viewport_y0, m_viewport_width, m_viewport_height;
		float			m_pixel_scale;

		rgba			m_background_color;
		float			m_timer;
		int			m_mouse_x, m_mouse_y, m_mouse_buttons;
		void *			m_userdata;
		movie::drag_state	m_drag_state;	// @@ fold this into m_mouse_button_state?
		mouse_button_state	m_mouse_button_state;
		// SwiffPlayer.dll: non-owning +0x34
		// capture pointer (0x10073560, 0x1007FBF0, 0x1007FC00).
		movie*	m_active_mouse_entity = NULL;
		bool			m_on_event_load_called;

		movie_root(movie_def_impl* def)
			:
			m_def(def),
			m_movie(NULL),
			m_viewport_x0(0),
			m_viewport_y0(0),
			m_viewport_width(1),
			m_viewport_height(1),
			m_pixel_scale(1.0f),
			m_background_color(0, 0, 0, 255),
			m_timer(0.0f),
			m_mouse_x(0),
			m_mouse_y(0),
			m_mouse_buttons(0),
			m_userdata(NULL),
			m_on_event_load_called(false)
		{
			assert(m_def != NULL);

			// SwiffPlayer.dll 0x10073560 reads height before width, with
			// truncating dword conversions before setting the viewport.
			movie_def_impl* const definition = m_def.get_ptr();
			const int height = bf2x87::toIntTruncate(definition->get_height_pixels());
			const int width = bf2x87::toIntTruncate(definition->get_width_pixels());
			set_display_viewport(0, 0, width, height);
		}

		// SwiffPlayer.dll 0x10075370 / deleting entry 0x10075340:
		// release the movie before the definition, through the native owners above.
		~movie_root()
		{
			assert(m_def != NULL);
			// [CHOSEN, not recovered] S3 bridge to issue #7: the native
			// wrapper also owns its sprite. Drop the published root before that
			// wrapper dies, without clearing a newer movie's publication.
			if (get_global_root_movie() == m_movie.get_ptr()) set_global_root_movie(NULL);
			m_movie = NULL;
			m_def = NULL;
		}

		// @@ should these delegate to m_movie?  Probably...
		virtual void	set_member(const tu_stringi& name, const as_value& val) {}
		virtual bool	get_member(const tu_stringi& name, as_value* val) { return false; }
		virtual movie*	to_movie() { assert(0); return 0; } // @@ should this return m_movie.get_ptr()?
		

		void	set_root_movie(movie* root_movie)
		{
			m_movie = root_movie;
			assert(m_movie != NULL);
		}

		// SwiffPlayer.dll 0x100716B0.
		void	set_display_viewport(int x0, int y0, int w, int h)
		{
			m_viewport_x0 = x0;
			m_viewport_y0 = y0;
			m_viewport_width = w;
			m_viewport_height = h;

			// Recompute pixel scale.
			const bf2x87::Real scale_x = bf2x87::div(bf2x87::Real(m_viewport_width),
				bf2x87::mul(bf2x87::sub(bf2x87::Real(m_def->m_frame_size.m_x_max),
					bf2x87::Real(m_def->m_frame_size.m_x_min)), bf2x87::Real(0.05f)));
			const float scale_y = bf2x87::store(bf2x87::div(bf2x87::Real(m_viewport_height),
				bf2x87::mul(bf2x87::sub(bf2x87::Real(m_def->m_frame_size.m_y_max),
					bf2x87::Real(m_def->m_frame_size.m_y_min)), bf2x87::Real(0.05f))));
			m_pixel_scale = bf2x87::less(scale_x, bf2x87::Real(scale_y)) ? scale_y : bf2x87::store(scale_x);
		}


		// SwiffPlayer.dll 0x10073670.
		void	notify_mouse_state(int x, int y, int buttons)
		// The host app uses this to tell the movie where the
		// user's mouse pointer is.
		{
			m_mouse_x = x;
			m_mouse_y = y;
			m_mouse_buttons = buttons;
		}

		// SwiffPlayer.dll 0x10073690.
		virtual void	get_mouse_state(int* x, int* y, int* buttons)
		// Use this to retrieve the last state of the mouse, as set via
		// notify_mouse_state().  Coordinates are in PIXELS, NOT TWIPS.
		{
			assert(x);
			assert(y);
			assert(buttons);

			*x = m_mouse_x;
			*y = m_mouse_y;
			*buttons = m_mouse_buttons;
		}

		// Capture since the press. Retail 0x1007FBF0/0x1007FC00 reads/writes a raw
		// pointer in movie_root.
		movie*	get_active_entity() { return m_active_mouse_entity; }
		void	set_active_entity(movie* m) { m_active_mouse_entity = m; }

		movie*	get_root_movie() { return m_movie.get_ptr(); }


		// SwiffPlayer.dll 0x1007FD00 reaches this record through the sprite's root.
		void	stop_drag()
		{
			m_drag_state.m_character = NULL;
		}


		// SwiffPlayer.dll 0x10073770; native per-tree root delegate.
		movie_definition*	get_movie_definition() { return m_movie->get_movie_definition(); }

		uint32	get_file_bytes() const
		{
			return m_def->get_file_bytes();
		}

		// SwiffPlayer.dll 0x10073780.
		// 0-based!!
		int	get_current_frame() const { return m_movie->get_current_frame(); }
		float	get_frame_rate() const { return m_def->get_frame_rate(); }

		// SwiffPlayer.dll 0x100736B0.
		virtual float	get_pixel_scale() const
		// Return the size of a logical movie pixel as
		// displayed on-screen, with the current device
		// coordinates.
		{
			return m_pixel_scale;
		}

		// @@ Is this one necessary?
		character*	get_character(int character_id)
		{
			return m_movie->get_character(character_id);
		}

		// SwiffPlayer.dll 0x100736D0.
		void	set_background_color(const rgba& color)
		{
			m_background_color = color;
		}

		// SwiffPlayer.dll 0x100736C0; the native API copies the stored color.
		rgba	get_background_color() const
		{
			return m_background_color;
		}

		// SwiffPlayer.dll 0x100736E0.
		void	set_background_alpha(float alpha)
		{
			const float rounded = bf2x87::store(bf2x87::add(
				bf2x87::mul(bf2x87::Real(alpha), bf2x87::Real(255.0f)), bf2x87::Real(0.5f)));
			m_background_color.m_a = iclamp(bf2x87::toIntTruncate(rounded), 0, 255);
		}

		// SwiffPlayer.dll 0x10073720.
		float	get_background_alpha() const
		{
			return m_background_color.m_a * (1.0f / 255.0f);
		}

		float	get_timer() const { return m_timer; }

		// SwiffPlayer.dll 0x10073790. Root delegates use the native per-tree
		// owner in place of retail's global root [CHOSEN, not recovered] S3 bridge.
		void	restart() { m_movie->restart(); }
		
		void	advance(float delta_time)
		{
			if (m_on_event_load_called == false)
			{
				// Must do loading events.  For child sprites this is
				// done by the dlist, but root movies don't get added
				// to a dlist, so we do it here.
				m_on_event_load_called = true;
				m_movie->on_event_load();
			}
			// SwiffPlayer.dll 0x1007AED0 calls 0x10077910 before mouse input
			// and before adding delta_time to the movie clock.
			if (s_profiling)
			{
				static profile_record counter("updateIntervals");
				counter.begin();
				update_intervals(m_timer);
				counter.end();
			}
			else update_intervals(m_timer);
			
			
			// @@ TODO handle multi-frame catch-up stuff
			// here, and make it optional.  Make
			// movie::advance() a fixed framerate w/ no
			// dt.

			// Handle the mouse.  one recursive pass down the tree, every
			// character its own state machine (sprite 0x10080010, text 0x10088650, button 0x100A3020).
			// Stock 2006 picked a single "topmost entity" here and fed generate_mouse_button_events.
			m_movie->mouse_update(false);

			// SwiffPlayer.dll 0x1007AED0 calls the
			// mouse pass before incrementing the movie clock, then advances the timeline.
			m_timer += delta_time;
			m_movie->advance(delta_time);
		}

		// 0-based!!
		// SwiffPlayer.dll 0x100737A0 calls primary +0x18 (0x100822A0),
		// distinct from the sprite's secondary numeric queue entry.
		void	goto_frame(int target_frame_number);

		// SwiffPlayer.dll 0x100737B0.
		virtual bool	has_looped() const { return m_movie->has_looped(); }

		// SwiffPlayer.dll 0x10075160. The per-tree movie is the existing
		// [CHOSEN, not recovered] S3 bridge for retail's global root.
		void	display()
		{
			if (m_movie->get_visible() == false)
			{
				// Don't display.
				return;
			}

			gameswf::render::begin_display(
				m_background_color,
				m_viewport_x0, m_viewport_y0,
				m_viewport_width, m_viewport_height,
				m_def->m_frame_size.m_x_min, m_def->m_frame_size.m_x_max,
				m_def->m_frame_size.m_y_min, m_def->m_frame_size.m_y_max);

			if (s_profiling)
			{
				static profile_record counter("display");
				// The timer abstraction uses QueryPerformanceCounter on Windows,
				// as retail 0x100D0BA0 does. Its existing POSIX clock backend is
				// [CHOSEN, not recovered] platform policy; counters stay in ticks.
				counter.m_start = tu_timer::get_profile_ticks();
				m_movie->display();
				counter.m_ticks += tu_timer::get_profile_ticks() - counter.m_start;
				++counter.m_count;
			}
			else
			{
				m_movie->display();
			}

			gameswf::render::end_display();
		}

		virtual bool	goto_labeled_frame(const char* label)
		{
			// SwiffPlayer.dll 0x100737C0 resolves against the root definition.
			int frame = -1;
			if (m_def->get_labeled_frame(label, &frame))
			{
				goto_frame(frame);
				return true;
			}
			log_error("error: movie_impl::goto_labeled_frame('%s') unknown label\n", label);
			return false;
		}

		// SwiffPlayer.dll 0x10073810.
		virtual void	set_play_state(play_state s) { m_movie->set_play_state(s); }
		// SwiffPlayer.dll 0x10073820.
		virtual play_state	get_play_state() const { return m_movie->get_play_state(); }

		/* movie_root */
		// SwiffPlayer.dll 0x10073830.
		virtual void	set_variable(const char* path_to_var, const char* new_value)
		{
			m_movie->set_variable(path_to_var, new_value);
		}

		/* movie_root */
		// SwiffPlayer.dll 0x10073840.
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value)
		{
			m_movie->set_variable(path_to_var, new_value);
		}

		// SwiffPlayer.dll 0x10073850.
		virtual const char*	get_variable(const char* path_to_var) const
		{
			return m_movie->get_variable(path_to_var);
		}

		/*movie_root*/
		// SwiffPlayer.dll 0x10073870 / 0x10073860 forward one command string.
		// [CHOSEN, not recovered] S3 bridge: the native instance owns its root
		// instead of dispatching through retail's process-global root pointer.
		const char* call_method(const Uint16* expression) override
		{
			return m_movie->call_method(expression);
		}
		const char* call_method(const char* expression) override
		{
			return m_movie->call_method(expression);
		}

		// SwiffPlayer.dll 0x10073880 / 0x10073890.
		virtual void	set_visible(bool visible) { m_movie->set_visible(visible); }
		virtual bool	get_visible() const { return m_movie->get_visible(); }

		// SwiffPlayer.dll 0x10073750 / 0x10073760.
		virtual void * get_userdata() { return m_userdata; }
		virtual void set_userdata(void * ud ) { m_userdata = ud;  }

	};


	static void define_font_info_2_loader(stream* in, int tag_type, movie_definition_sub* m);

	// SwiffPlayer.dll 0x1007CF60.
	static void	ensure_loaders_registered()
	{
		static bool	s_registered = false;
	
		if (s_registered == false)
		{
			// Register the standard loaders.
			s_registered = true;
			register_tag_loader(0, end_loader);
			register_tag_loader(2, define_shape_loader);
			register_tag_loader(4, place_object_2_loader);
			register_tag_loader(6, define_bits_jpeg_loader);
			register_tag_loader(7, button_character_loader);
			register_tag_loader(8, jpeg_tables_loader);
			register_tag_loader(9, set_background_color_loader);
			register_tag_loader(10, define_font_loader);
			register_tag_loader(11, define_text_loader);
			register_tag_loader(12, do_action_loader);
			register_tag_loader(13, define_font_info_loader);
			register_tag_loader(14, define_sound_loader);
			register_tag_loader(15, start_sound_loader);
			register_tag_loader(17, button_sound_loader);
			register_tag_loader(20, define_bits_lossless_2_loader);
			register_tag_loader(21, define_bits_jpeg2_loader);
			register_tag_loader(22, define_shape_loader);
			register_tag_loader(24, null_loader);	// "protect" tag; we're not an authoring tool so we don't care.
			register_tag_loader(26, place_object_2_loader);
			register_tag_loader(28, remove_object_2_loader);
			register_tag_loader(32, define_shape_loader);
			register_tag_loader(33, define_text_loader);
			register_tag_loader(37, define_edit_text_loader);
			register_tag_loader(34, button_character_loader);
			register_tag_loader(35, define_bits_jpeg3_loader);
			register_tag_loader(36, define_bits_lossless_2_loader);
			register_tag_loader(39, sprite_loader);
			register_tag_loader(43, frame_label_loader);
			register_tag_loader(46, define_shape_morph_loader);
			register_tag_loader(48, define_font_loader);
			register_tag_loader(56, export_loader);
			register_tag_loader(57, import_loader);
			register_tag_loader(59, do_init_action_loader);   
			register_tag_loader(62, define_font_info_2_loader);
		}
	}



	// SwiffPlayer.dll 0x1006F380.
	// [CHOSEN, not recovered] The host still uses the extra native tag-count output.
	void	get_movie_info(
		const char* filename,
		int* version,
		int* width,
		int* height,
		float* frames_per_second,
		int* frame_count,
		int* tag_count
		)
	// Attempt to read the header of the given .swf movie file.
	// Put extracted info in the given vars.
	// Sets *version to 0 if info can't be extracted.
	{
		if (s_opener_function == NULL)
		{
			log_error("error: get_movie_info(): no file opener function registered\n");
			if (version) *version = 0;
			return;
		}

		tu_file*	in = s_opener_function(filename);
		if (in == NULL || in->get_error() != TU_FILE_NO_ERROR)
		{
			log_error("error: get_movie_info(): can't open '%s'\n", filename);
			if (version) *version = 0;
			delete in;
			return;
		}

		Uint32	file_start_pos = in->get_position();
		Uint32	header = in->read_le32();
		Uint32	file_length = in->read_le32();
		Uint32	file_end_pos = file_start_pos + file_length;

		int	local_version = (header >> 24) & 255;
		if ((header & 0x0FFFFFF) != 0x00535746
		    && (header & 0x0FFFFFF) != 0x00535743)
		{
			// ERROR
			log_error("error: get_movie_info(): file '%s' does not start with a SWF header!\n", filename);
			if (version) *version = 0;
			delete in;
			return;
		}
		bool	compressed = (header & 255) == 'C';

		if (compressed)
		{
			log_error("get_movie_info(): can't read zipped SWF data; TU_CONFIG_LINK_TO_ZLIB is 0!\n");
			return;
		}

		stream	str(in);

		rect	frame_size;
		frame_size.read(&str);

		float	local_frame_rate = str.read_u16() / 256.0f;
		int	local_frame_count = str.read_u16();

		if (version) *version = local_version;
		if (width) *width = bf2x87::toIntTruncate(
			bf2x87::add(bf2x87::mul(bf2x87::sub(bf2x87::Real(frame_size.m_x_max),
				bf2x87::Real(frame_size.m_x_min)), bf2x87::Real(0.05f)), bf2x87::Real(0.5f)));
		if (height) *height = bf2x87::toIntTruncate(
			bf2x87::add(bf2x87::mul(bf2x87::sub(bf2x87::Real(frame_size.m_y_max),
				bf2x87::Real(frame_size.m_y_min)), bf2x87::Real(0.05f)), bf2x87::Real(0.5f)));
		if (frames_per_second) *frames_per_second = local_frame_rate;
		if (frame_count) *frame_count = local_frame_count;

		if (tag_count)
		{
			// Count tags.
			int local_tag_count = 0;
			while ((Uint32) str.get_position() < file_end_pos)
			{
				str.open_tag();
				str.close_tag();
				local_tag_count++;
			}
			*tag_count = local_tag_count;
		}

		delete in;
	}



	// see gameswf.h. Default preserves stock behaviour.
	static create_bitmaps_flag	s_default_create_bitmaps = DO_LOAD_BITMAPS;

	void	set_default_create_bitmaps(create_bitmaps_flag cbf)
	{
		s_default_create_bitmaps = cbf;
	}

	// SwiffPlayer.dll 0x1007E010: tail entry to create_movie_sub.
	movie_definition*	create_movie(const char* filename)
	// Create the movie definition from the specified .swf file.
	{
		return create_movie_sub(filename);
	}


	// SwiffPlayer.dll 0x1007D3C0. The default bitmap policy remains a native
	// [CHOSEN, not recovered] S3 bridge; retail constructs both load flags as zero.
	movie_definition_sub*	create_movie_sub(const char* filename)
	{
		if (s_opener_function == NULL)
		{
			// Don't even have a way to open the file.
			log_error("error: no file opener function; can't create movie.  "
				  "See gameswf::register_file_opener_callback\n");
			return NULL;
		}

		tu_file* in = s_opener_function(filename);
		if (in == NULL)
		{
			log_error("failed to open '%s'; can't create movie.\n", filename);
			return NULL;
		}
		else if (in->get_error())
		{
			log_error("error: file opener can't open '%s'\n", filename);
			return NULL;
		}

		ensure_loaders_registered();

		// was hardcoded DO_LOAD_BITMAPS - see set_default_create_bitmaps.
		movie_def_impl*	m = new movie_def_impl(s_default_create_bitmaps, DO_LOAD_FONT_SHAPES);
		m->read(in);

		delete in;

		if (m && s_use_cache_files)
		{
			// Try to load a .gsc file.
			tu_string	cache_filename(filename);
			cache_filename += ".gsc";
			tu_file*	cache_in = s_opener_function(cache_filename.c_str());
			if (cache_in == NULL
			    || cache_in->get_error() != TU_FILE_NO_ERROR)
			{
				// Can't open cache file; don't sweat it.
				IF_VERBOSE_PARSE(log_msg("note: couldn't open cache file '%s'\n", cache_filename.c_str()));

				// SwiffPlayer.dll 0x1007D4BA..0x1007D4DC only reports a
				// missing cache; it does not generate font textures here.
			}
			else
			{
				// Load the cached data.
				m->input_cached_data(cache_in);
			}

			delete cache_in;
		}

		m->add_ref();
		return m;
	}


	static bool	s_no_recurse_while_loading = false;	// @@ TODO get rid of this; make it the normal mode.


	// SwiffPlayer.dll 0x1007D520.
	movie_definition*	create_movie_no_recurse(
		tu_file* in,
		create_bitmaps_flag cbf,
		create_font_shapes_flag cfs)
	{
		ensure_loaders_registered();

		// @@ TODO make no_recurse the standard way to load.
		// In create_movie(), use the visit_ API to keep
		// visiting dependent movies, until everything is
		// loaded.  That way we only have one code path and
		// the resource_proxy stuff gets tested.
		s_no_recurse_while_loading = true;

		movie_def_impl*	m = new movie_def_impl(cbf, cfs);
		m->read(in);

		s_no_recurse_while_loading = false;

		m->add_ref();
		return m;
	}


	//
	// global gameswf management
	//


	// For built-in sprite ActionScript methods.
	as_object*	s_sprite_builtins = 0;	// shared among all sprites.
	// SwiffPlayer.dll 0x1007FB20 / 0x10084B60; source-level name [INFERRED].
	static Uint32 s_sprite_live_count = 0;
	static void	sprite_builtins_init();
	static void	sprite_builtins_clear();


	void	clear()
	// Maximum release of resources.
	{
		clear_library();
		sprite_builtins_clear();
		fontlib::clear();
		action_clear();
	}


	//
	// library stuff, for sharing resources among different movies.
	//


	static stringi_hash< smart_ptr<movie_definition_sub> >	s_movie_library;
	static hash< movie_definition_sub*, smart_ptr<movie_interface> >	s_movie_library_inst;
	static array<movie_interface*> s_extern_sprites;
	static movie_interface* s_current_root;

	static tu_string s_workdir;

	void	release_library_classes()
	// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. The library
	// keeps its definitions for the next player; their class bindings belong to the released
	// movie, and a stale class's constructor would run in an environment that no longer exists.
	{
		for (stringi_hash< smart_ptr<movie_definition_sub> >::iterator it = s_movie_library.begin(); it != s_movie_library.end(); ++it)
		{
			if (it->second != NULL)
			{
				it->second->release_registered_classes();
			}
		}
	}

	// SwiffPlayer.dll 0x10075440 appends a borrowed external-movie pointer.
	void save_extern_movie(movie_interface* m)
	{
		s_extern_sprites.push_back(m);
	}

//#if 0
	void delete_unused_root()
	{
		for (int i = 0; i < s_extern_sprites.size(); i++)
		{
			movie_interface* root_m = s_extern_sprites[i];
			movie* m = root_m->get_root_movie();
      
			if (m->get_ref_count() < 2)
			{
				IF_VERBOSE_ACTION(log_msg("extern movie deleted\n"));
				s_extern_sprites.remove(i);
				i--;
				root_m->drop_ref();
			}
		}
	}
//#endif // 0

	// SwiffPlayer.dll 0x1006F560.
	movie_interface* get_current_root()
	{
		assert(s_current_root != NULL);
		return s_current_root;
	}

	// SwiffPlayer.dll 0x1006F570.
	void set_current_root(movie_interface* m)
	{
		assert(m != NULL);
		s_current_root = m;
	}

	// SwiffPlayer.dll 0x1006F580 / 0x1006F590.
	// [CHOSEN, not recovered] S3 bridge: native string storage replaces the
	// retail fixed buffer; callers still borrow it until the next assignment.
	const char* get_workdir()
	{
		return s_workdir.c_str();
	}

	void set_workdir(const char* dir)
	{
		assert(dir != NULL);
		s_workdir = dir;
	}

	void	clear_library()
	// Drop all library references to movie_definitions, so they
	// can be cleaned up.
	{
		// SwiffPlayer.dll 0x10076700 clears definitions before instances.
		s_movie_library.clear();
		s_movie_library_inst.clear();
	}

// tulrich: Vitaly sent this code.  I don't trust it though!  It looks
// like it may be dropping refs in order to remove cyclic references.
// I would rather fix the source of any cyclic references directly.
//
// 	void clear_library()
// 	// Drop all library references to movie_definitions, so they
// 	// can be cleaned up.
// 	{
// 		{for (hash< movie_definition_sub*, smart_ptr<movie_interface> >::iterator it =
// 			      s_movie_library_inst.begin();
// 		      it != s_movie_library_inst.end();
// 		      ++it)
// 		{
// 			smart_ptr<movie_interface> obj = it->second;
// 			while (obj->get_ref_count() > 2)            
// 			{                               
// 				obj->drop_ref();
// 			}
// 		}}
// 		s_movie_library_inst.clear();
//   
// 		{for (stringi_hash< smart_ptr<movie_definition_sub> >::iterator it = s_movie_library.begin();
// 		      it != s_movie_library.end();                                                            
// 		      ++it)                        
// 		{
// 			smart_ptr<movie_definition_sub> obj = it->second;
// 			while (obj->get_ref_count() > 2)                 
// 			{                               
// 				obj->drop_ref();
// 			}
// 		}}
// 		s_movie_library.clear();
// 	}


	// SwiffPlayer.dll 0x1007E020: tail entry to create_library_movie_sub.
	movie_definition*	create_library_movie(const char* filename)
	// Try to load a movie from the given url, if we haven't
	// loaded it already.  Add it to our library on success, and
	// return a pointer to it.
	{
		return create_library_movie_sub(filename);
	}


	// SwiffPlayer.dll 0x1007D580.
	movie_definition_sub*	create_library_movie_sub(const char* filename)
	{
		tu_string	fn(filename);

		// Is the movie already in the library?
		{
			smart_ptr<movie_definition_sub>	m;
			s_movie_library.get(fn, &m);
			if (m != NULL)
			{
				// Return cached movie.
				m->add_ref();
				return m.get_ptr();
			}
		}

		// Try to open a file under the filename.
		movie_definition_sub*	mov = create_movie_sub(filename);

		if (mov == NULL)
		{
			log_error("error: couldn't load library movie '%s'\n", filename);
			return NULL;
		}
		else
		{
			s_movie_library.add(fn, mov);
		}

		mov->add_ref();
		return mov;
	}
	
	movie_interface*	create_library_movie_inst(movie_definition* md)
	{
		return create_library_movie_inst_sub((movie_definition_sub*)md);
	}


	// SwiffPlayer.dll 0x10075470; forwarding entry 0x10076720.
	movie_interface*	create_library_movie_inst_sub(movie_definition_sub* md)
	{
		// Is the movie instance already in the library?
		{
			smart_ptr<movie_interface>	m;
			s_movie_library_inst.get(md, &m);
			if (m != NULL)
			{
				// Return cached movie instance.
				m->add_ref();
				return m.get_ptr();
			}
		}

		// Try to create movie interface
		movie_interface* mov = md->create_instance();

		if (mov == NULL)
		{
			log_error("error: couldn't create instance\n");

			return NULL;
		}
		else
		{
			s_movie_library_inst.add(md, mov);
		}

		mov->add_ref();
		return mov;
	}


	// SwiffPlayer.dll 0x10071720; the handler save/restore helpers are
	// 0x1006F5B0 / 0x1006F5E0.
	void	precompute_cached_data(movie_definition* movie_def)
	// Fill in cached data in movie_def.
	// @@@@ NEEDS TESTING -- MIGHT BE BROKEN!!!
	{
		assert(movie_def != NULL);

		// Temporarily install null render and sound handlers,
		// so we don't get output during preprocessing.
		//
		// Use automatic struct var to make sure we restore
		// when exiting the function.
		struct save_stuff
		{
			render_handler*	m_original_rh;
			sound_handler*	m_original_sh;

			// SwiffPlayer.dll 0x1006F5B0.
			save_stuff()
			{
				// Save.
				m_original_rh = get_render_handler();
				m_original_sh = get_sound_handler();
				set_render_handler(NULL);
				set_sound_handler(NULL);
			}

			// SwiffPlayer.dll 0x1006F5E0.
			~save_stuff()
			{
				// Restore.
				set_render_handler(m_original_rh);
				set_sound_handler(m_original_sh);
			}
		} save_stuff_instance;

		// Need an instance.
		gameswf::movie_interface*	m = movie_def->create_instance();
		if (m == NULL)
		{
			log_error("error: precompute_cached_data can't create instance of movie\n");
			return;
		}
		
		// Run through the movie's frames.
		//
		// @@ there might be cleaner ways to do this; e.g. do
		// execute_frame_tags(i, true) on movie and all child
		// sprites.
		int	kick_count = 0;
		for (;;)
		{
			// @@ do we also have to run through all sprite frames
			// as well?
			//
			// @@ also, ActionScript can rescale things
			// dynamically -- we can't really do much about that I
			// guess?
			//
			// @@ Maybe we should allow the user to specify some
			// safety margin on scaled shapes.

			int	last_frame = m->get_current_frame();
			m->advance(0.010f);
			m->display();

			// SwiffPlayer.dll 0x100717A5 / 0x10071839 queries the definition first.
			const int frame_count = movie_def->get_frame_count();
			if (m->get_current_frame() == frame_count - 1)
			{
				// Done.
				break;
			}

			if (m->get_play_state() == gameswf::movie_interface::STOP)
			{
				// Kick the movie.
				//printf("kicking movie, kick ct = %d\n", kick_count);
				m->goto_frame(last_frame + 1);
				m->set_play_state(gameswf::movie_interface::PLAY);
				kick_count++;

				if (kick_count > 10)
				{
					//printf("movie is stalled; giving up on playing it through.\n");
					break;
				}
			}
			else if (m->get_current_frame() < last_frame)
			{
				// Hm, apparently we looped back.  Skip ahead...
				log_error("loop back; jumping to frame %d\n", last_frame);
				m->goto_frame(last_frame + 1);
			}
			else
			{
				kick_count = 0;
			}
		}

		m->drop_ref();
	}


	//
	// Some tag implementations
	//


	// SwiffPlayer.dll 0x1006F600 is an empty tag loader.
	void	null_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Silently ignore the contents of this tag.
	{
	}

	// SwiffPlayer.dll 0x1006F610.
	void	frame_label_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Label the current frame of m with the name from the stream.
	{
		char*	n = in->read_string();
		m->add_frame_name(n);
		delete [] n;
	}

	struct set_background_color : public execute_tag
	{
		// SwiffPlayer.dll 0x10071900: implicit destruction has no owned members.
		// SwiffPlayer.dll 0x10071890: implicit rgba construction sets every component to 255.
		rgba	m_color;

		// SwiffPlayer.dll 0x100718B0 stores once after multiply and add,
		// then truncates the float argument and keeps its low byte.
		void	execute(movie* m)
		{
			float	current_alpha = m->get_background_alpha();
			const float rounded = bf2x87::store(bf2x87::add(
				bf2x87::mul(bf2x87::Real(current_alpha), bf2x87::Real(255.0f)), bf2x87::Real(0.5f)));
			m_color.m_a = static_cast<Uint8>(bf2x87::toIntTruncate(rounded));
			m->set_background_color(m_color);
		}

		// SwiffPlayer.dll 0x100718F0.
		void	execute_state(movie* m)
		{
			execute(m);
		}

		// SwiffPlayer.dll 0x1006F640.
		void	read(stream* in)
		{
			m_color.read_rgb(in);
		}
	};


	// SwiffPlayer.dll 0x100738F0.
	void	set_background_color_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 9);
		assert(m);

		set_background_color*	t = new set_background_color;
		t->read(in);

		m->add_execute_tag(t);
	}

#if 0
	// Bitmap character
	struct bitmap_character : public bitmap_character_def
	{
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

		gameswf::bitmap_info*	get_bitmap_info()
		{
			return m_bitmap_info.get_ptr();
		}

	private:
		smart_ptr<gameswf::bitmap_info>	m_bitmap_info;
	};
#endif

	// SwiffPlayer.dll 0x1006F660 is empty in the shipped JPEG-disabled build.
	void	jpeg_tables_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load JPEG compression tables that can be used to load
	// images further along in the stream.
	{
		assert(tag_type == 8);

	}


	// SwiffPlayer.dll 0x10073A50/0x10073B80/0x10073C60/0x10073D40.
	// These loaders create a named-host-image descriptor without decoding payloads.
	static bitmap_character* make_movie_bitmap_character(int id)
	{
		named_bitmap_info* bi = new named_bitmap_info;
		bi->m_texture_id = static_cast<unsigned>(-1);
		bi->m_original_width = 256;
		bi->m_original_height = 256;
		bitmap_character* ch = new bitmap_character(bi);
		// [CHOSEN, not recovered] S3 bridge: native constructor ID adapter.
		ch->set_character_id(id);
		return ch;
	}

	// SwiffPlayer.dll 0x10073A50.
	void	define_bits_jpeg_loader(stream* in, int tag_type, movie_definition_sub* m)
	// A JPEG image without included tables; those should be in an
	// existing jpeg::input object stored in the movie.
	{
		assert(tag_type == 6);

		Uint16	character_id = in->read_u16();

		m->add_bitmap_character(character_id, make_movie_bitmap_character(character_id));
	}


	// SwiffPlayer.dll 0x10073B80.
	void	define_bits_jpeg2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 21);
		
		Uint16	character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("define_bits_jpeg2_loader: charid = %d pos = 0x%x\n", character_id, in->get_position()));

		m->add_bitmap_character(character_id, make_movie_bitmap_character(character_id));
	}




	// SwiffPlayer.dll 0x10073C60.
	void	define_bits_jpeg3_loader(stream* in, int tag_type, movie_definition_sub* m)
	// loads a define_bits_jpeg3 tag. This is a jpeg file with an alpha
	// channel using zlib compression.
	{
		assert(tag_type == 35);

		Uint16	character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("define_bits_jpeg3_loader: charid = %d pos = 0x%x\n", character_id, in->get_position()));

		m->add_bitmap_character(character_id, make_movie_bitmap_character(character_id));
	}


	// SwiffPlayer.dll 0x10073D40.
	void	define_bits_lossless_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 20 || tag_type == 36);

		Uint16	character_id = in->read_u16();
		m->add_bitmap_character(character_id, make_movie_bitmap_character(character_id));
	}


	// SwiffPlayer.dll 0x1006F670.
	void	define_shape_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 2
		       || tag_type == 22
		       || tag_type == 32);

		Uint16	character_id = in->read_u16();
		shape_character_def*	ch = new shape_character_def;
		// [CHOSEN, not recovered] S3 bridge: the native constructor has
		// no ID argument; retain retail's definition ID before reading.
		ch->set_character_id(character_id);
		ch->read(in, tag_type, true, m);

		IF_VERBOSE_PARSE(log_msg("shape_loader: id = %d, rect ", character_id); ch->get_bound().print());

		m->add_character(character_id, ch);
	}

	// SwiffPlayer.dll 0x1006F700.
	void define_shape_morph_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 46);
		Uint16 character_id = in->read_u16();
		IF_VERBOSE_PARSE(log_msg("shape_morph_loader: id = %d\n", character_id));
		morph2_character_def* morph = new morph2_character_def;
		// [CHOSEN, not recovered] S3 bridge: constructor ID adapter, as above.
		morph->set_character_id(character_id);
		morph->read(in, tag_type, true, m);
		m->add_character(character_id, morph);
	}

	//
	// font loaders
	//


	// SwiffPlayer.dll 0x1006F780.
	void	define_font_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load a DefineFont or DefineFont2 tag.
	{
		assert(tag_type == 10 || tag_type == 48);

		Uint16	font_id = in->read_u16();
		
		font*	f = new font;
		f->read(in, tag_type, m);

		m->add_font(font_id, f);

		// Automatically keeping fonts in fontlib is
		// problematic.  The app should be responsible for
		// optionally adding fonts to fontlib.
		// //fontlib::add_font(f);
	}


	// SwiffPlayer.dll 0x1006F7E0.
	void	define_font_info_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load a DefineFontInfo tag.  This adds information to an
	// existing font.
	{
		assert(tag_type == 13);

		Uint16	font_id = in->read_u16();
		
		font*	f = m->get_font(font_id);
		if (f)
		{
			f->read_font_info(in);
		}
		else
		{
			log_error("define_font_info_loader: can't find font w/ id %d\n", font_id);
		}
	}

	// SwiffPlayer.dll 0x1006F820 uses the same reader as tag 13, including
	// its lack of a separate language-byte read for DefineFontInfo2.
	static void define_font_info_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 62);
		const Uint16 font_id = in->read_u16();
		font* f = m->get_font(font_id);
		if (f)
		{
			f->read_font_info(in);
		}
		else
		{
			log_error("define_font_info_2_loader: can't find font w/ id %d\n", font_id);
		}
	}


	//
	// swf_event
	//
	// For embedding event handlers in place_object_2


	struct swf_event
	{
		// NOTE: DO NOT USE THESE AS VALUE TYPES IN AN
		// array<>!  They cannot be moved!  The private
		// operator=(const swf_event&) should help guard
		// against that.

		event_id	m_event;
		action_buffer	m_action_buffer;
		as_value	m_method;

		swf_event()
		{
		}

		// SwiffPlayer.dll 0x1007B470; event IDs retain the S2 mapping.
		void	attach_to(character* ch) const
		{
			ch->set_event_handler(m_event, m_method);
		}

		// SwiffPlayer.dll 0x10073E00.
		void	read(stream* in, Uint32 flags)
		{
			assert(flags != 0);

			// Scream if more than one bit is set, since we're not set up to handle
			// that, and it doesn't seem possible to express in ActionScript source,
			// so it's important to know if this ever occurs in the wild.
			if (flags & (flags - 1))
			{
				log_error("error: swf_event::read() -- more than one event type encoded!  "
					  "unexpected! flags = 0x%x\n", flags);
			}

			// [CHOSEN, not recovered] S2 bridge: native event IDs differ;
			// retain the shipped 21-entry bit mapping using the native names.
			static const event_id	s_code_bits[21] =
			{
				event_id::LOAD,
				event_id::ENTER_FRAME,
				event_id::UNLOAD,
				event_id::MOUSE_MOVE,
				event_id::MOUSE_DOWN,
				event_id::MOUSE_UP,
				event_id::KEY_DOWN,
				event_id::KEY_UP,
				event_id::DATA,
				event_id::INITIALIZE,
				event_id::PRESS,
				event_id::RELEASE,
				event_id::RELEASE_OUTSIDE,
				event_id::ROLL_OVER,
				event_id::ROLL_OUT,
				event_id::DRAG_OVER,
				event_id::DRAG_OUT,
				event_id::KEY_PRESS,	// bit 17 (still unhandled below)
				event_id::CONSTRUCT,	// bit 18
				event_id::DOUBLE_PRESS,
				event_id::INVALID,
			};

			for (int i = 0, mask = 1; i < int(sizeof(s_code_bits)/sizeof(s_code_bits[0])); i++, mask <<= 1)
			{
				if (flags & mask)
				{
					m_event = s_code_bits[i];
					break;
				}
			}

			// what to do w/ key_press???  Is the data in the reserved parts of the flags???
			if (flags & (1 << 17))
			{
				log_error("swf_event::read -- KEY_PRESS found, not handled yet, flags = 0x%x\n", flags);
			}

			Uint32	event_length = in->read_u32();
			UNUSED(event_length);

			// Read the actions.
			IF_VERBOSE_ACTION(log_msg("---- actions for event %s\n", m_event.get_function_name().c_str()));
			m_action_buffer.read(in);

			if (m_action_buffer.get_length() != (int) event_length)
			{
				log_error("error -- swf_event::read(), event_length = %d, but read %d\n",
					  event_length,
					  m_action_buffer.get_length());
				// @@ discard this event handler??
			}

			// Create a function to execute the actions.
			array<with_stack_entry>	empty_with_stack;
			// [CHOSEN, not recovered] S3 bridge: the native function
			// constructor carries the extra (empty) with-stack parameter.
			as_as_function*	func = new as_as_function(&m_action_buffer, NULL, 0, empty_with_stack);
			func->set_length(m_action_buffer.get_length());

			m_method.set_as_as_function(func);
		}

	private:
		// DON'T USE THESE
		swf_event(const swf_event& s) { assert(0); }
		void	operator=(const swf_event& s) { assert(0); }
	};


	//
	// place_object_2
	//
	
	struct place_object_2 : public execute_tag
	{
		// [CHOSEN, not recovered] S3 bridge for 2006 reverse-tag APIs;
		// retail 0x10076760 does not retain the input tag number.
		int	m_tag_type;
		char*	m_name;
		float	m_ratio;
		cxform	m_color_transform;
		matrix	m_matrix;
		bool	m_has_matrix;
		bool	m_has_cxform;
		Uint16	m_depth;
		Uint16	m_character_id;
                Uint16 	m_clip_depth;
		enum place_type {
			PLACE,
			MOVE,
			REPLACE,
		} m_place_type;
		array<swf_event*>	m_event_handlers;


		// SwiffPlayer.dll 0x100755E0; m_tag_type is the native bridge above.
		place_object_2()
			:
			m_tag_type(0),
			m_name(NULL),
			m_ratio(0),
			m_has_matrix(false),
			m_has_cxform(false),
			m_depth(0),
			m_character_id(0),
                        m_clip_depth(0),
			m_place_type(PLACE)
		{
		}

		// SwiffPlayer.dll 0x1007B4C0; deleting veneer 0x1007B490.
		~place_object_2()
		{
			delete [] m_name;
			m_name = NULL;

			for (int i = 0, n = m_event_handlers.size(); i < n; i++)
			{
				delete m_event_handlers[i];
			}
			m_event_handlers.resize(0);
		}

		// SwiffPlayer.dll 0x10076760.
		void	read(stream* in, int tag_type, int movie_version)
		{
			assert(tag_type == 4 || tag_type == 26);

			m_tag_type = tag_type;

			if (tag_type == 4)
			{
				// Original place_object tag; very simple.
				m_character_id = in->read_u16();
				m_depth = in->read_u16();
				m_matrix.read(in);

				if (in->get_position() < in->get_tag_end_position())
				{
					m_color_transform.read_rgb(in);
				}
			}
			else if (tag_type == 26)
			{
				in->align();

				bool	has_actions = in->read_uint(1) ? true : false;
				bool	has_clip_bracket = in->read_uint(1) ? true : false;
				bool	has_name = in->read_uint(1) ? true : false;
				bool	has_ratio = in->read_uint(1) ? true : false;
				bool	has_cxform = in->read_uint(1) ? true : false;
				bool	has_matrix = in->read_uint(1) ? true : false;
				bool	has_char = in->read_uint(1) ? true : false;
				bool	flag_move = in->read_uint(1) ? true : false;

				m_depth = in->read_u16();

				if (has_char) {
					m_character_id = in->read_u16();
				}

				if (has_matrix) {
					m_has_matrix = true;
					m_matrix.read(in);
				}
				if (has_cxform) {
					m_has_cxform = true;
					m_color_transform.read_rgba(in);
				}
				
				if (has_ratio) {
					m_ratio = in->read_u16() * (1.0f / 65535.0f);
				}
				
				if (has_name) {
					m_name = in->read_string();
				}
				if (has_clip_bracket) {
					m_clip_depth = in->read_u16(); 
					IF_VERBOSE_PARSE(log_msg("HAS CLIP BRACKET!\n"));
				}
				if (has_actions)
				{
					Uint16	reserved = in->read_u16();
					UNUSED(reserved);

					// The logical 'or' of all the following handlers.
					// I don't think we care about this...
					Uint32	all_flags = 0;
					if (movie_version >= 6)
					{
						all_flags = in->read_u32();
					}
					else
					{
						all_flags = in->read_u16();
					}
					UNUSED(all_flags);

					// Read swf_events.
					for (;;)
					{
						// Read event.
						in->align();

						Uint32	this_flags = 0;
						if (movie_version >= 6)
						{
							this_flags = in->read_u32();
						}
						else
						{
							this_flags = in->read_u16();
						}

						if (this_flags == 0)
						{
							// Done with events.
							break;
						}

						swf_event*	ev = new swf_event;
						ev->read(in, this_flags);

						m_event_handlers.push_back(ev);
					}
				}


				if (has_char == true && flag_move == true)
				{
					// Remove whatever's at m_depth, and put m_character there.
					m_place_type = REPLACE;
				}
				else if (has_char == false && flag_move == true)
				{
					// Moves the object at m_depth to the new location.
					m_place_type = MOVE;
				}
				else if (has_char == true && flag_move == false)
				{
					// Put m_character at m_depth.
					m_place_type = PLACE;
				}
                                
				IF_VERBOSE_PARSE(log_msg("po2r: name = %s\n", m_name));
				IF_VERBOSE_PARSE(log_msg("po2r: char id = %d, mat:\n", m_character_id); m_matrix.print());
			}
		}

		
		// SwiffPlayer.dll 0x10075640.
		void	execute(movie* m)
		// Place/move/whatever our object in the given movie.
		{
			switch (m_place_type)
			{
			case PLACE:
				m->add_display_object(
					m_character_id,
					m_name,
					m_event_handlers,
					m_depth,
					m_tag_type != 4,	// original place_object doesn't do replacement
					m_color_transform,
					m_matrix,
					m_ratio,
					m_clip_depth);
				IF_VERBOSE_DEBUG(log_msg(">>>>>>>>> character (%s) placed at %d\n", m_name, m_depth));
				break;

			case MOVE:
				m->move_display_object(
					m_depth,
					m_has_cxform,
					m_color_transform,
					m_has_matrix,
					m_matrix,
					m_ratio,
					m_clip_depth);
				IF_VERBOSE_DEBUG(log_msg(">>>>>>>>> character (%s) moved at %d\n", m_name, m_depth));
				break;

			case REPLACE:
				m->replace_display_object(
					m_character_id,
					m_name,
					m_depth,
					m_has_cxform,
					m_color_transform,
					m_has_matrix,
					m_matrix,
					m_ratio,
					m_clip_depth);
				IF_VERBOSE_DEBUG(log_msg(">>>>>>>>> character (%s) replaced at %d\n", m_name, m_depth));
				break;
			}
		}

		// SwiffPlayer.dll 0x10075770 delegates to execute.
		void	execute_state(movie* m)
		{
			execute(m);
		}

		//  see execute_tag::execute_goto: the goto pass does nothing
		// with a place (0x10075760 is a nullsub); the state builder reads its depth and kind.
		// SwiffPlayer.dll 0x10075760.
		void	execute_goto(movie* /*m*/) {}
		// SwiffPlayer.dll 0x10075630.
		bool	is_place_tag() const { return true; }
		int	get_tag_depth() const { return m_depth; }
		int	get_place_type() const { return (int) m_place_type; }

		void	execute_state_reverse(movie* m, int frame)
		{
			switch (m_place_type)
			{
			case PLACE:
				// reverse of add is remove
				m->remove_display_object(m_depth);
				break;

			case MOVE:
				// reverse of move is move
				m->move_display_object(
					m_depth,
					m_has_cxform,
					m_color_transform,
					m_has_matrix,
					m_matrix,
					m_ratio,
					m_clip_depth);
				break;

			case REPLACE:
			{
				// reverse of replace is to re-add the previous object.
				execute_tag*	last_add = m->find_previous_replace_or_add_tag(frame, m_depth, -1);
				if (last_add)
				{
					last_add->execute_state(m);
				}
				else
				{
					log_error("reverse REPLACE can't find previous replace or add tag(%d, %d)\n",
						  frame, m_depth);
					
				}
				break;
			}
			}
		}

		virtual uint32	get_depth_id_of_replace_or_add_tag() const
		// "depth_id" is the 16-bit depth & id packed into one 32-bit int.
		{
			if (m_place_type == PLACE || m_place_type == REPLACE)
			{
				int	id = -1;
				if (m_tag_type == 4)
				{
					// Old-style PlaceObject; the corresponding Remove
					// is specific to the character_id.
					id = m_character_id;
				}
				return ((m_depth & 0x0FFFF) << 16) | (id & 0x0FFFF);
			}
			else
			{
				return (uint32) -1;
			}
		}
	};


	
	// SwiffPlayer.dll 0x10076A30.
	void	place_object_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 4 || tag_type == 26);

		place_object_2*	ch = new place_object_2;
		ch->read(in, tag_type, m->get_version());

		m->add_execute_tag(ch);
	}


	//
	// sprite_definition
	//


	// A sprite is a mini movie-within-a-movie.  It doesn't define
	// its own characters; it uses the characters from the parent
	// movie, but it has its own frame counter, display list, etc.
	//
	// The sprite implementation is divided into a
	// sprite_definition and a sprite_instance.  The _definition
	// holds the immutable data for a sprite, while the _instance
	// contains the state for a specific instance being updated
	// and displayed in the parent movie's display list.


	// SwiffPlayer.dll ctor 0x1007B5A0: movie_definition_sub at +0 (table 0x100E0138) and character_def at
	// +8 (table 0x100E0108), sharing the virtual ref_counted at +0x40.
	struct sprite_definition : public movie_definition_sub, public character_def
	{
		movie_definition_sub*	     m_movie_def;		// parent movie.
		array<array<execute_tag*> >  m_playlist;	// movie control events for each frame.
		array<execute_tag*> m_init_action_list;
		stringi_hash<int>	     m_named_frames;	// stores 0-based frame #'s
		int	                     m_frame_count;
		int	                     m_loading_frame;
		sprite_definition(movie_definition_sub* m, int character_id = 0)
			:
			m_movie_def(m),
			m_frame_count(0),
			m_loading_frame(0)
		{
			// SwiffPlayer.dll 0x1007B5A0.
			set_character_id(character_id);
			assert(m_movie_def);
		}

		// SwiffPlayer.dll 0x1007D730 (deleting entry 0x1007D700):
		// delete playlist tags in frame order;
		// member destruction releases labels, init storage, then playlist storage.
		~sprite_definition()
		{
			// Release our playlist data.
			{for (int i = 0, n = m_playlist.size(); i < n; i++)
			{
				for (int j = 0, m = m_playlist[i].size(); j < m; j++)
				{
					delete m_playlist[i][j];
				}
			}}
		}

		// overloads from movie_definition
		// SwiffPlayer.dll 0x1006F970: definitions do not display their timelines.
		virtual void	display(character* instance_info) {}

		// SwiffPlayer.dll 0x1007B670 returns four zero floats even for a nonempty
		// sprite definition; live sprite bounds come from its display list.
		rect	get_bound_copy() const override
		{
			rect bounds;
			bounds.m_x_min = 0.0f;
			bounds.m_x_max = 0.0f;
			bounds.m_y_min = 0.0f;
			bounds.m_y_max = 0.0f;
			return bounds;
		}

		// SwiffPlayer.dll 0x1007B6A0.
		virtual float	get_width_pixels() const { return 1; }
		// SwiffPlayer.dll 0x1007B6B0.
		virtual float	get_height_pixels() const { return 1; }
		// SwiffPlayer.dll 0x1007B6C0.
		virtual int	get_frame_count() const { return m_frame_count; }
		// SwiffPlayer.dll 0x1007B6D0.
		virtual float	get_frame_rate() const { return m_movie_def->get_frame_rate(); }
		// SwiffPlayer.dll 0x1007B6E0.
		virtual int	get_loading_frame() const { return m_loading_frame; }
		// SwiffPlayer.dll 0x1007B6F0.
		virtual int	get_version() const { return m_movie_def->get_version(); }
		// SwiffPlayer.dll 0x1007B700.
		virtual void	add_character(int id, character_def* ch) { log_error("add_character tag appears in sprite tags!\n"); }
		// SwiffPlayer.dll 0x1007B710.
		virtual void	add_font(int id, font* ch) { log_error("add_font tag appears in sprite tags!\n"); }
		// SwiffPlayer.dll 0x1007B720.
		virtual font*	get_font(int id) { return m_movie_def->get_font(id); }
		// SwiffPlayer.dll 0x1007B730.
		virtual void	set_jpeg_loader(jpeg::input* j_in) { assert(0); }
		// SwiffPlayer.dll 0x1007B740.
		virtual jpeg::input*	get_jpeg_loader() { return NULL; }
		// SwiffPlayer.dll 0x1007B750.
		virtual bitmap_character_def*	get_bitmap_character(int id) { return m_movie_def->get_bitmap_character(id); }
		// SwiffPlayer.dll 0x1007B760.
		virtual void	add_bitmap_character(int id, bitmap_character_def* ch) { log_error("add_bc appears in sprite tags!\n"); }
		// SwiffPlayer.dll 0x1007B770.
		virtual sound_sample*	get_sound_sample(int id) { return m_movie_def->get_sound_sample(id); }
		// SwiffPlayer.dll 0x1007B780.
		virtual void	add_sound_sample(int id, sound_sample* sam) { log_error("add sam appears in sprite tags!\n"); }

		// @@ would be nicer to not inherit these...
		// SwiffPlayer.dll 0x1007B790.
		virtual create_bitmaps_flag	get_create_bitmaps() const { assert(0); return DO_LOAD_BITMAPS; }
		// SwiffPlayer.dll 0x1007B7A0.
		virtual create_font_shapes_flag	get_create_font_shapes() const { assert(0); return DO_LOAD_FONT_SHAPES; }
		// SwiffPlayer.dll 0x1007B7B0.
		virtual int	get_bitmap_info_count() const { assert(0); return 0; }
		// SwiffPlayer.dll 0x1007B7C0.
		virtual bitmap_info*	get_bitmap_info(int i) const { assert(0); return NULL; }
		// SwiffPlayer.dll 0x1007B7D0.
		virtual void	add_bitmap_info(bitmap_info* bi) { assert(0); }

		// SwiffPlayer.dll 0x1007B7E0.
		virtual void	export_resource(int id, const tu_string& symbol, resource* res) { log_error("can't export from sprite\n"); }
		// SwiffPlayer.dll 0x1007B7F0.
		virtual bool get_export_name(int id, tu_string* out) { return m_movie_def->get_export_name(id, out); }
		// SwiffPlayer.dll 0x1007B8B0.
		virtual smart_ptr<resource>	get_exported_resource(const tu_string& sym) { return m_movie_def->get_exported_resource(sym); }
		virtual bool	get_exported_font_name(const font* f, tu_string* out) { return m_movie_def->get_exported_font_name(f, out); }	//
		// SwiffPlayer.dll 0x1007B800.
		virtual void	add_import(const char* source_url, int id, const char* symbol) { assert(0); }
		// SwiffPlayer.dll 0x1007B810.
		virtual void	visit_imported_movies(import_visitor* v) { assert(0); }
		// SwiffPlayer.dll 0x1007B820.
		virtual void	resolve_import(const char* source_url, movie_definition* d) { assert(0); }
		// SwiffPlayer.dll 0x1007B830.
		virtual character_def*	get_character_def(int id)
		{
			return m_movie_def->get_character_def(id);
		}
		// SwiffPlayer.dll 0x1007B840.
		virtual void	generate_font_bitmaps() { assert(0); }


		// SwiffPlayer.dll 0x1007B850; secondary-interface thunk 0x1007B890.
		virtual void	output_cached_data(tu_file* out, const cache_options& options)
		{
			// Nothing to do.
			return;
		}
		// SwiffPlayer.dll 0x1007B860; secondary-interface thunk 0x1007B8A0.
		virtual void	input_cached_data(tu_file* in)
		{
			// Nothing to do.
			return;
		}

		// SwiffPlayer.dll 0x1007B870.
		virtual movie_interface*	create_instance()
		{
			return NULL;
		}

		// overloads from character_def
		virtual character*	create_character_instance(movie* parent, int id);


		/* sprite_definition */
		// SwiffPlayer.dll 0x1007B8F0.
		virtual void	add_execute_tag(execute_tag* c)
		{
			m_playlist[m_loading_frame].push_back(c);
		}

		// SwiffPlayer.dll 0x10071630 ignores init actions inside a sprite.
		virtual void	add_init_action(execute_tag* c)
		{
		}

		/* sprite_definition */
		// SwiffPlayer.dll 0x1007B990.
		virtual void	add_frame_name(const char* name)
		// Labels the frame currently being loaded with the
		// given name.  A copy of the name string is made and
		// kept in this object.
		{
			assert(m_loading_frame >= 0 && m_loading_frame < m_frame_count);

			tu_string	n = name;
			int	currently_assigned = 0;
			if (m_named_frames.get(n, &currently_assigned) == true)
			{
				log_error("add_frame_name(%d, '%s') -- frame name already assigned to frame %d; overriding\n",
					  m_loading_frame,
					  name,
					  currently_assigned);
			}
			m_named_frames.set(n, m_loading_frame);	// stores 0-based frame #
		}

		/* sprite_definition */
		// SwiffPlayer.dll 0x1007B920.
		bool	get_labeled_frame(const char* label, int* frame_number)
		// Returns 0-based frame #
		{
			return m_named_frames.get(label, frame_number);
		}

		// SwiffPlayer.dll 0x1007B8D0.
		const array<execute_tag*>&	get_playlist(int frame_number)
		// frame_number is 0-based
		{
			return m_playlist[frame_number];
		}

		// SwiffPlayer.dll 0x1007B880 returns the empty sprite init-action list.
		virtual const array<execute_tag*>*	get_init_actions(int frame_number)
		{
			return &m_init_action_list;
		}


		// SwiffPlayer.dll 0x100777C0.
		void	read(stream* in)
		// Read the sprite info.  Consists of a series of tags.
		{
			int	tag_end = in->get_tag_end_position();

			m_frame_count = in->read_u16();
			m_playlist.resize(m_frame_count);	// need a playlist for each frame

			IF_VERBOSE_PARSE(log_msg("sprite: frames = %d\n", m_frame_count));

			m_loading_frame = 0;

			while ((Uint32) in->get_position() < (Uint32) tag_end)
			{
				int	tag_type = in->open_tag();
				loader_function lf = NULL;
				if (tag_type == 1)
				{
					// show frame tag -- advance to the next frame.
					m_loading_frame++;
				}
				else if (s_tag_loaders.get(tag_type, &lf))
				{
					// call the tag loader.  The tag loader should add
					// characters or tags to the movie data structure.
					(*lf)(in, tag_type, this);
				}
				else
				{
					// no tag loader for this tag type.
					IF_VERBOSE_PARSE(log_msg("*** no tag loader for type %d\n", tag_type));
				}

				in->close_tag();
			}

		}
	};


	//
	// sprite_instance
	//

	// SwiffPlayer.dll 0x10080590/0x10083A20 compare built-in property
	// spellings exactly. Keep the 2006 enum as a local dispatch adapter.
	static as_standard_member sprite_standard_member(const tu_stringi& name)
	{
		static const char* const names[] = {
			"_x", "_y", "_xscale", "_yscale", "_currentframe", "_totalframes",
			"_alpha", "_visible", "_width", "_height", "_rotation", "_target",
			"_framesloaded", "_name", "_droptarget", "_url", "_highquality",
			"_focusrect", "_soundbuftime", "_xmouse", "_ymouse", "_parent"
		};
		for (int i = 0; i < ARRAYSIZE(names); ++i)
		{
			if (strcmp(name.c_str(), names[i]) == 0)
			{
				return static_cast<as_standard_member>(i);
			}
		}
		return M_INVALID_MEMBER;
	}


	// [TEMP] RW-88 navigation tracer: a clip's dotted path, walked up to the root. The existing
	// attachMovie line prints get_name() alone, which is empty for every clip the components build,
	// so a page swap could not be attributed to a container. get_parent() answers a movie*, so each
	// step casts back down with cast_to_character(). Measured 2026-09-11 on the Manage Accounts
	// overlap; remove with the rest of the RW-88 tracing.
	static const char*	swf_trace_path(character* ch)
	{
		static char	buf[256];
		const char*	parts[16];
		int		n = 0;
		for (character* c = ch; c && n < 16; )
		{
			parts[n++] = c->get_name().c_str();
			movie* p = c->get_parent();
			c = p ? p->cast_to_character() : NULL;
		}
		int	len = 0;
		buf[0] = '\0';
		for (int i = n - 1; i >= 0 && len < (int) sizeof(buf) - 2; i--)
		{
			const char* p = (parts[i] && parts[i][0]) ? parts[i] : "?";
			len += ::_snprintf_s(buf + len, sizeof(buf) - len, _TRUNCATE, "%s%s", len ? "." : "", p);
		}
		return buf;
	}

	// [TEMP] RW-89 mouse tracer: the press/release lines the interactive drives read, in the format
	// of the stock-era tracer that lived beside generate_mouse_button_events. [RETIRE-WHEN: RW-89]
#if !defined(_MSC_VER)
	// [TEMP] [CHOSEN, not recovered] BF2_SWF_EMPTY_LIST_GUARD=1 ends a sprite's mouse child
	// pass when a handler emptied its list; off by default. Retail 0x10080270 has no guard.
	// [RETIRE-WHEN: swf-empty-list-trigger] Remove after the maintainer identifies the
	// emptying handler and validates the retail-equivalent path with this flag unset.
	static bool	swf_empty_list_guard()
	{
		static int	s_on = -1;
		if (s_on < 0)
		{
			const char*	v = getenv("BF2_SWF_EMPTY_LIST_GUARD");
			s_on = (v && v[0] == '1') ? 1 : 0;
		}
		return s_on != 0;
	}

	// [TEMP] c-swf-disconnect: the tracer's path, for the other gameswf files.
	const char*	swf_list_trace_path(character* ch) { return swf_trace_path(ch); }
#endif

	static void	swf_trace_mouse(const char* what, character* ch)
	{
		log_error("[mouse] %s -> %s\n", what, swf_trace_path(ch));
	}

	// [TEMP] [CHOSEN, not recovered] Input-only diagnostics never inspect script values.
	// [RETIRE-WHEN: ea-popup-input] Remove after live login/Close input is verified.
	static bool swf_trace_input_enabled()
	{
		const char* value = ::getenv("BF2_SWIFFPLAYER_INPUTLOG");
		return value && value[0] == '1' && value[1] == '\0';
	}
	static bool s_trace_mouse_pass = false;

	// [CHOSEN, not recovered] The root-definition reads. Retail keeps a sprite's definition as the
	// character_def pointer at sprite +0xC8 and reads the ID (+0x10, duplicateMovieClip 0x100759CF) and
	// the registered class (+0x14, on_event_load 0x10081911) through it even for a ROOT sprite, whose
	// definition is a movie_def_impl and no character_def. Those offsets are movie_def_impl members:
	// 0x10077FB0 zeroes them (ID 0, no class), and once the movie has loaded they may hold container
	// state, which retail then reads as an ID or a class - undefined. The native types do not overlap,
	// so a root sprite answers the constructor's values: no sprite_definition, ID 0, no class.
	static sprite_definition* sprite_definition_of(movie_definition_sub* def)
	{
		return dynamic_cast<sprite_definition*>(def);
	}

	struct sprite_instance : public character
	{
		smart_ptr<movie_definition_sub>	m_def;
		movie_root*	m_root;

		display_list	m_display_list;
		array<action_buffer*>	m_action_list;

		play_state	m_play_state;
		int		m_current_frame;
		int		m_next_frame;
		float		m_time_remainder;
		bool		m_update_frame;
		bool		m_force_frame_update;
		bool		m_first_frame_initialized;
		bool		m_has_looped;
		bool	        m_accept_anim_moves;	// once we've been moved by ActionScript, don't accept moves from anim tags.

		as_environment	m_as_environment;

		// [CHOSEN, not recovered] S2 bridge; SwiffPlayer.dll 0x1007FB60
		// returns this sprite's environment through its secondary interface.
		virtual as_environment*	get_action_environment() { return &m_as_environment; }

		enum mouse_state
		{
			UP = 0,
			DOWN,
			OVER
		};
		mouse_state m_mouse_state;

		// SwiffPlayer.dll 0x10081870 creates a private script object for a bound class.
		// [CHOSEN, not recovered] S3 bridge: native as_object storage and strong
		// ownership replace the retail secondary-interface object at +0x104.
		// [RETIRE-WHEN: swf-native-script-object] Retire after common script-object
		// integration resolves movie-valued constructor/accessor prototypes and
		// native GC ownership, and class accessors/events pass menu and reload gates.
		smart_ptr<as_object>	m_script_object;

		// SwiffPlayer.dll 0x1007F3AB initializes the postincrement depth counter to 1000.
		int	m_next_highest_depth;
		// SwiffPlayer.dll 0x1007F433; a negative index is absent from member lookup.
		int	m_tab_index;
		// [UNRESOLVED] The constructor stores 1000 at primary +0x114 in
		// addition to the depth counter at +0xDC. Keep the separate word;
		// its meaning is not established by the inspected consumers.
		Uint32 m_retail_word_114;

		// the drawing-API canvas - the shape definition behind the
		// `_____defaultShape` child the shipped sub_1007C0D0 creates at depth 0 on first use.
		smart_ptr<shape_character_def>	m_canvas;

		//  numeric gotos waiting for advance() (the shipped sprite's
		// pending_goto_frames array at +264, drained by 0x10081CF0).
		array<int>	m_pending_goto_frames;

		//  the shipped sprite's mouse state (+0x2150 previous, +0x2154 current:
		// bit 0 inside, bit 1 a button down), its first-frame latch (+255) and whether it took the
		// press it still holds (+256).
		int	m_mouse_flags, m_prev_mouse_flags;
		bool	m_first_mouse_frame;
		bool	m_mouse_pressed;

		// SwiffPlayer.dll 0x1007F310. Native character/definition/environment
		// storage uses their existing [CHOSEN, not recovered] S3 ownership bridges.
		sprite_instance(movie_definition_sub* def, movie_root* r, movie* parent, int id)
			:
			character(parent, id),
			m_def(def),
			m_root(r),
			m_play_state(PLAY),
			m_current_frame(0),
			m_next_frame(0),
			m_time_remainder(0),
			m_update_frame(true),
			m_force_frame_update(false),
			m_first_frame_initialized(false),
			m_has_looped(false),
			m_accept_anim_moves(true),
			m_mouse_state(UP),
			m_mouse_flags(0),
			m_prev_mouse_flags(0),
			m_first_mouse_frame(true),
			m_mouse_pressed(false),
			m_next_highest_depth(1000),
			m_tab_index(-1),
			m_retail_word_114(1000),
			m_dead(false)	//
		{
			assert(m_def != NULL);
			assert(m_root != NULL);
			
			//m_root->add_ref();	// @@ circular!
			m_as_environment.set_target(this);
			// SwiffPlayer.dll 0x1007F310 preloads these undefined environment members.
			// Class handlers precede the environment in get_member (0x10080590).
			static const char* const event_names[] = {
				"onPress", "onDoublePress", "onRelease", "onReleaseOutside",
				"onRollOver", "onRollOut", "onDragOver", "onDragOut", "onKeyPress",
				"onInitialize", "onLoad", "onUnload", "onEnterFrame", "onMouseDown",
				"onMouseUp", "onMouseMove", "onKeyDown", "onKeyUp", "onData"
			};
			for (int i = 0; i < ARRAYSIZE(event_names); ++i)
			{
				m_as_environment.set_member(event_names[i], as_value());
			}

			sprite_builtins_init();
			++s_sprite_live_count;
		}

		bool	m_dead;	//  see character::mark_dead
		virtual void	mark_dead() { m_dead = true; }

		virtual void gc_mark()
		{
			// SwiffPlayer.dll 0x10080F90: locals, members, referenced children,
			// private script object. Registers, operands and handlers are omitted.
			if (m_gc_generation == get_gc_mark_generation()) return;
			m_gc_generation = get_gc_mark_generation();
			for (int i = 0; i < m_as_environment.m_local_frames.size(); ++i)
				gc_mark_value(m_as_environment.m_local_frames[i].m_value);
			for (stringi_hash<as_value>::iterator it = m_as_environment.m_variables.begin();
				it != m_as_environment.m_variables.end(); ++it)
				gc_mark_value(it->second);
			const int count = m_display_list.get_character_count();
			for (int i = 0; i < count; ++i)
				if (character* child = m_display_list.get_character(i)) child->gc_mark();
			if (m_script_object != NULL) m_script_object->gc_mark();
		}

		virtual void gc_release()
		{
			// SwiffPlayer.dll 0x10081BB0: referenced children, OBJECT members,
			// member-bag clear, private script object. Locals are retained.
			if (m_gc_generation == get_gc_release_generation()) return;
			m_gc_generation = get_gc_release_generation();
			const int count = m_display_list.get_character_count();
			for (int i = 0; i < count; ++i)
				if (character* child = m_display_list.get_character(i)) child->gc_release();
			for (stringi_hash<as_value>::iterator it = m_as_environment.m_variables.begin();
				it != m_as_environment.m_variables.end(); ++it)
				if (it->second.get_type() == as_value::OBJECT && it->second.to_object())
					it->second.to_object()->gc_release();
			m_as_environment.m_variables.clear();
			if (m_script_object != NULL) m_script_object->gc_release();
		}

		// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. Adds the
		// environment, the private script object and every child to the base character's values.
		virtual void	release_script_values(array<as_value>* held)
		{
			character::release_script_values(held);
			m_as_environment.release_values(held);
			if (m_script_object != NULL)
			{
				held->push_back(as_value(m_script_object.get_ptr()));
				m_script_object = NULL;
			}
			m_display_list.release_characters(held);
		}

		// SwiffPlayer.dll 0x10084B60; deleting wrapper 0x10084B20.
		// Native definition/environment/base ownership retains its S3 bridges.
		virtual ~sprite_instance()
		{
			// SwiffPlayer.dll 0x10084B60 decrements before touching children.
			--s_sprite_live_count;
			// 0x10084B92..0x10084BB8 releases capture held by this sprite.
			movie* owner = get_global_root_movie();
			if (owner && owner->get_active_mouse_entity() == this)
			{
				get_global_root_movie()->set_active_mouse_entity(NULL);
			}
			// 0x10084BC5..0x10084C05: sweep pending removals, then detach
			// each remaining child before dropping its display-list reference.
			m_display_list.update();
			while (m_display_list.get_character_count() != 0)
			{
				character* child = m_display_list.get_character(0);
				if (child)
				{
					m_display_list.remove_display_object(child->get_depth());
					m_display_list.update();
				}
			}
			m_display_list.clear();
			//m_root->drop_ref();
		}

		// SwiffPlayer.dll 0x1007FB80 / 0x1007FB70.
		movie_interface*	get_root_interface() { return m_root; }
		movie_root*	get_root() { return m_root; }
		movie*	get_root_movie() { return m_root->get_root_movie(); }

		// SwiffPlayer.dll 0x100803F0 copies presentation state before definition/root.
		virtual void	copy_display_state(character* target) const
		{
			character::copy_display_state(target);
			sprite_instance* sprite = static_cast<sprite_instance*>(target);
			sprite->m_def = m_def;
			sprite->m_root = m_root;
		}

		// SwiffPlayer.dll 0x1007FDC0.
		movie_definition*	get_movie_definition() { return m_def.get_ptr(); }

		// SwiffPlayer.dll 0x1007FDD0: referenced children only, snapshot count,
		// local matrix copied before the child virtual; first child seeds the union.
		rect get_bound_copy() const override
		{
			rect bounds = { 0, 0, 0, 0 };
			bool first = true;
			for (int i = 0, n = m_display_list.get_character_count(); i < n; ++i)
			{
				character* child = m_display_list.get_character(i);
				if (child == NULL) continue;
				const matrix transform = child->get_matrix();
				rect current = child->get_bound_copy();
				current.enclose_transformed_rect(transform, current);
				if (first) { bounds = current; first = false; }
				else bounds.expand_to_rect(current);
			}
			return bounds;
		}

		// SwiffPlayer.dll 0x10081150: registration and ending clock/count are
		// unconditional, while only the starting clock is gated on profiling.
		float get_width()
		{
			static profile_record counter("sprite_get_width");
			if (s_profiling) counter.begin();
			const rect bounds = get_bound_copy();
			const float width = (float)(((long double)bounds.m_x_max - bounds.m_x_min) * 0.05f);
			counter.end();
			return width;
		}

		// SwiffPlayer.dll 0x10081230, including the same profiling quirk.
		float get_height()
		{
			static profile_record counter("sprite_get_height");
			if (s_profiling) counter.begin();
			const rect bounds = get_bound_copy();
			const float height = (float)(((long double)bounds.m_y_max - bounds.m_y_min) * 0.05f);
			counter.end();
			return height;
		}

		// [CHOSEN, not recovered] S3 bridge: the retained 2006 local-extent API
		// returns twips; the retail pixel accessors above use the same rectangle.
		virtual float	get_local_width()
		{
			return get_bound_copy().width();
		}
		virtual float	get_local_height()
		{
			return get_bound_copy().height();
		}

		// SwiffPlayer.dll 0x1007FB90.
		int	get_current_frame() const { return m_current_frame; }
		// SwiffPlayer.dll 0x100719E0.
		int	get_frame_count() const { return m_def->get_frame_count(); }

		// SwiffPlayer.dll 0x1007FF30.
		void	set_play_state(play_state s)
		// Stop or play the sprite.
		{
			if (m_play_state != s)
			{
				m_time_remainder = 0;
			}

			m_play_state = s;
			if (s == STOP)
			{
				m_next_frame = m_current_frame + 1;
				if (m_next_frame >= m_def->get_frame_count()) m_next_frame = 0;
			}
		}
		// SwiffPlayer.dll 0x1007FBA0.
		play_state	get_play_state() const { return m_play_state; }


		// SwiffPlayer.dll 0x1007FBB0: no character-ID lookup on a live sprite.
		character*	get_character(int character_id)
		{
//			return m_def->get_character_def(character_id);
			// @@ TODO -- look through our dlist for a match
			return NULL;
		}

		// SwiffPlayer.dll 0x1007FD60: snapshot the child count, skip deferred
		// removals, and forward the same index without updating child sprites' members.
		virtual void	set_tab_index(int index)
		{
			for (int i = 0, n = m_display_list.get_character_count(); i < n; ++i)
			{
				character* ch = m_display_list.get_character(i);
				if (ch) ch->set_tab_index(index);
			}
		}

		// SwiffPlayer.dll 0x1007FB40: inherit a parent's index only for the
		// propagation interface; get_member("tabIndex") tests this clip's own value.
		virtual int	get_tab_index() const
		{
			if (m_tab_index >= 0) return m_tab_index;
			return m_parent ? m_parent->get_tab_index() : -1;
		}

		// SwiffPlayer.dll 0x1007FBC0.
		float	get_background_alpha() const
		{
			// @@ this doesn't seem right...
			return m_root->get_background_alpha();
		}

		// SwiffPlayer.dll 0x1007FBD0.
		float	get_pixel_scale() const { return m_root->get_pixel_scale(); }

		virtual void	get_mouse_state(int* x, int* y, int* buttons)
		{
			m_root->get_mouse_state(x, y, buttons);
		}

		// SwiffPlayer.dll 0x1007FC20.
		rgba	get_background_color() const { return m_root->get_background_color(); }

		// SwiffPlayer.dll 0x1007FC30.
		void	set_background_color(const rgba& color)
		{
			m_root->set_background_color(color);
		}

		// SwiffPlayer.dll 0x1007FC40.
		float	get_timer() const { return m_root->get_timer(); }

		// SwiffPlayer.dll 0x1007FC50 resets timeline and first-mouse state only.
		void	restart()
		{
			m_first_mouse_frame = true;
			m_current_frame = 0;
			m_next_frame = 0;
			m_time_remainder = 0;
			m_update_frame = true;
			m_has_looped = false;
		}


		// SwiffPlayer.dll 0x1007FC80.
		virtual bool	has_looped() const { return m_has_looped; }

		// SwiffPlayer.dll 0x1007FC90.
		virtual bool	get_accept_anim_moves() const { return m_accept_anim_moves; }

		inline int	transition(int a, int b) const
		// Combine the flags to avoid a conditional. It would be faster with a macro.
		{
			return (a << 2) | b;
		}


		//  sub_1007FF90 - does this sprite cover the world-space point: any
		// referenced, visible child that does, in display-list order. An invisible sprite covers nothing.
		virtual bool	point_test_world(float x, float y)
		{
			if (get_visible() == false)
			{
				return false;
			}
			for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
			{
				const display_object_info&	dobj = m_display_list.get_display_object(i);
				character*	ch = dobj.m_ref ? dobj.m_character.get_ptr() : NULL;
				if (ch != NULL && ch->get_visible() && ch->point_test_world(x, y))
				{
					return true;
				}
			}
			return false;
		}

		//  the root's mouse capture, kept on the movie_root.
		virtual movie*	get_active_mouse_entity() { return m_root->get_active_entity(); }
		virtual void	set_active_mouse_entity(movie* m) { m_root->set_active_entity(m); }

		//  sub_10080010 (709 instructions, read in full 2026-09-13) - the
		// sprite's mouse pass, vtable slot 4. Stock 2006 had none of this: movie_root picked ONE
		// "topmost entity" through get_topmost_mouse_entity (the first hit walking each display list
		// from the top, a bare shape included) and generate_mouse_button_events fed it. That is why a
		// click on the CREATE ACCOUNT page's buttons died in the closed country dropdown's list mask
		// drawn above them, and why a click on the e-mail field stopped at the Textbox's bevel shape.
		//
		// The shipped pass, per frame, per sprite:
		//
		//   1. its own state machine runs while this sprite holds the pointer, or while nobody does
		//      and this is not the root movie. `consumed` - a sibling above claimed the point - leaves
		//      both state bits clear, so the sprite sees "up, outside"; otherwise bit 1 is a button
		//      down and bit 0 is point_test_world on the pointer in world twips;
		//   2. the first frame only latches, and nothing fires unless m_mouse_enabled;
		//   3. the transitions, (current | previous << 2): 1 roll over; 4 roll out; 7 press - the
		//      capture taken, DOUBLE_PRESS when the host reports the buttons as 2, else MOUSE_DOWN then
		//      PRESS, and the capture dropped again when neither is handled; 13 and 9 with the press
		//      held: MOUSE_UP then RELEASE and the capture dropped, without it: roll over; 8 and 12
		//      with the press held: RELEASE_OUTSIDE and the capture dropped. Nothing else fires - a
		//      drag out or in is not an event for a sprite;
		//   4. the children, top-down, each fed the consumption so far and adding its own - unless
		//      this sprite holds the capture, in which case the children are skipped;
		//   5. the answer: consumed by a child, or inside-and-enabled here.
		//
		// The [mouse] press/release lines are the RW-89 tracer the interactive drives read [TEMP].
		virtual bool	mouse_update(bool consumed)
		{
			// SwiffPlayer.dll 0x10080010 holds
			// neither a self reference nor a reference to display-list storage.
#if !defined(_MSC_VER)
			// [TEMP] c-swf-disconnect: nesting of mouse passes for the [swf-list] tracer.
			struct pass_depth { pass_depth() { g_swf_mouse_pass_depth++; } ~pass_depth() { g_swf_mouse_pass_depth--; } } depth_scope;
#endif

			movie*	active = m_root->get_active_entity();
			const bool trace_root = this == m_root->get_root_movie() && swf_trace_input_enabled();
			if (trace_root)
			{
				static int previous_buttons = -1;
				int x, y, buttons;
				m_root->get_mouse_state(&x, &y, &buttons);
				s_trace_mouse_pass = buttons != previous_buttons;
				previous_buttons = buttons;
				if (s_trace_mouse_pass)
					log_error("[mouse] dispatch at=(%d,%d) buttons=%d capture=%s\n", x, y, buttons,
						active ? swf_trace_path(active->cast_to_character()) : "none");
			}
			if (active == this || (active == NULL && this != m_root->get_root_movie()))
			{
				m_prev_mouse_flags = m_mouse_flags;
				m_mouse_flags = 0;
				// SwiffPlayer.dll 0x10080068's virtual
				// world-matrix fetch. Its result is unused; retain the call and its order.
				matrix world = get_world_matrix();
				(void) world;
				int	mx = 0, my = 0, mb = 0;
				m_root->get_mouse_state(&mx, &my, &mb);
				if (!consumed)
				{
					if (mb)
					{
						m_mouse_flags |= 2;
					}
					if (point_test_world(PIXELS_TO_TWIPS((float) mx), PIXELS_TO_TWIPS((float) my)))
					{
						m_mouse_flags |= 1;
					}
				}
				if (m_first_mouse_frame)
				{
					m_prev_mouse_flags = m_mouse_flags;
					m_first_mouse_frame = false;
				}
				if (s_trace_mouse_pass && (m_mouse_flags & 1) && m_mouse_enabled)
					log_error("[mouse] hit %s id=%d depth=%d frame=%d flags=%u previous=%u dead=%d\n",
						swf_trace_path(this), get_id(), get_depth(), get_current_frame(),
						m_mouse_flags, m_prev_mouse_flags, m_dead);
				if (m_mouse_enabled && m_mouse_flags != m_prev_mouse_flags)
				{
					switch (m_mouse_flags | (m_prev_mouse_flags << 2))
					{
					case 13:
					case 9:
						if (m_mouse_pressed)
						{
							m_mouse_pressed = false;
							on_event(event_id::MOUSE_UP);
							swf_trace_mouse("release", this);
							on_event(event_id::RELEASE);
							m_root->set_active_entity(NULL);
						}
						else
						{
							on_event(event_id::ROLL_OVER);
						}
						break;
					case 7:
					{
						m_root->set_active_entity(this);
						m_mouse_pressed = true;
						bool	handled;
						if (mb == 2)
						{
							handled = on_event(event_id::DOUBLE_PRESS);
						}
						else
						{
							swf_trace_mouse("press", this);
							handled = on_event(event_id::MOUSE_DOWN);
							handled = on_event(event_id::PRESS) || handled;
						}
						if (!handled)
						{
							m_root->set_active_entity(NULL);
						}
						break;
					}
					case 1:
						on_event(event_id::ROLL_OVER);
						break;
					case 4:
						on_event(event_id::ROLL_OUT);
						break;
					case 8:
					case 12:
						if (m_mouse_pressed)
						{
							m_mouse_pressed = false;
							swf_trace_mouse("releaseOutside", this);
							on_event(event_id::RELEASE_OUTSIDE);
							m_root->set_active_entity(NULL);
						}
						break;
					default:
						break;
					}
				}
			}

			active = m_root->get_active_entity();
			if (active == NULL || active != this)
			{
				for (int i = m_display_list.get_character_count() - 1; i >= 0; i--)
				{
#if !defined(_MSC_VER)
					// [TEMP] c-swf-disconnect: the shipped loop (0x10080265..0x100802A2) caches the
					// count and re-reads the buffer; an emptied list leaves the buffer NULL and the
					// next read faults, in the shipped DLL as well. BF2_SWF_EMPTY_LIST_GUARD=1 (off by
					// default; not shipped behaviour) ends the pass there instead.
					if (m_display_list.get_character_count() <= i)
					{
						SWF_LIST_TRACE("loop '%s' i=%d past size %d%s", swf_trace_path(this), i,
							m_display_list.get_character_count(),
							m_display_list.get_character_count() == 0 ? " - EMPTY, buffer freed, next read faults" : " (stale slot)");
						if (m_display_list.get_character_count() == 0 && swf_empty_list_guard())
						{
							break;
						}
					}
#endif
					const display_object_info&	dobj = m_display_list.get_display_object(i);
					character*	ch = dobj.m_ref ? dobj.m_character.get_ptr() : NULL;
					if (ch != NULL && ch->get_visible())
					{
#if !defined(_MSC_VER)
						const int	trace_before = m_display_list.get_character_count();
#endif
						const bool was_consumed = consumed;
						consumed = ch->mouse_update(consumed) || consumed;
						if (s_trace_mouse_pass && consumed && !was_consumed)
							log_error("[mouse] consumed by %s id=%d depth=%d\n",
								swf_trace_path(ch), ch->get_id(), ch->get_depth());
#if !defined(_MSC_VER)
						if (m_display_list.get_character_count() != trace_before && swf_list_trace_on())
						{
							char	child_name[96];
							snprintf(child_name, sizeof(child_name), "%s", ch->get_name().c_str());
							SWF_LIST_TRACE("dlist CHANGED '%s' %d -> %d inside the child pass, after child #%d '%s'",
								swf_trace_path(this), trace_before,
								m_display_list.get_character_count(), i, child_name);
						}
#endif
					}
				}
			}
			if (trace_root) s_trace_mouse_pass = false;
			return consumed || ((m_mouse_flags & 1) != 0 && m_mouse_enabled);
		}


		/* sprite_instance */
		void	increment_frame_and_check_for_loop()
		// Increment m_current_frame, and take care of looping.
		{
			m_current_frame++;

			int	frame_count = m_def->get_frame_count();
			if (m_current_frame >= frame_count)
			{
				// Loop.
				m_current_frame = 0;
				m_has_looped = true;
				if (frame_count > 1)
				{
					m_display_list.reset();
				}
			}
		}

		// SwiffPlayer.dll 0x10081CF0. A queued goto consumes the incoming
		// delta; each pass sweeps and advances children after the parent's actions.
		virtual void	advance(float delta_time)
		{
			smart_ptr<as_object_interface> this_ptr(this);
			do
			{
				if (m_pending_goto_frames.size() > 0)
				{
					const int target = m_pending_goto_frames[0];
					m_pending_goto_frames.remove(0);
					apply_goto_frame(target);
					while (m_pending_goto_frames.size() > 0 && m_pending_goto_frames[0] == target)
					{
						m_pending_goto_frames.remove(0);
					}
					delta_time = 0.0f;
				}

				character::do_mouse_drag();
				if (m_play_state != STOP) m_time_remainder += delta_time;
				const float frame_time = 1.0f / m_root->get_frame_rate();
				const bool single_frame_movie = m_def->get_frame_count() == 1;
				if (m_time_remainder >= frame_time)
				{
					m_update_frame = true;
					m_time_remainder -= frame_time;
				}
				m_update_frame = m_update_frame || (m_play_state != STOP && m_force_frame_update);
				if (m_update_frame)
				{
					m_current_frame = m_next_frame;
					m_update_frame = false;
					m_next_frame = m_current_frame + 1;
					if (m_play_state == PLAY)
					{
						if (!single_frame_movie || !m_first_frame_initialized)
						{
							execute_frame_tags(m_current_frame);
							m_first_frame_initialized = true;
						}
						on_event(event_id::ENTER_FRAME);
						do_actions();
					}
				}
				m_display_list.update();
				m_display_list.advance(delta_time);
				if (m_next_frame >= m_def->get_frame_count())
				{
					m_next_frame = 0;
					m_has_looped = true;
				}
				if (m_time_remainder >= frame_time)
				{
					m_update_frame = true;
					m_time_remainder -= frame_time;
				}
			}
			while (m_pending_goto_frames.size() > 0);
		}

		// SwiffPlayer.dll 0x10081B10: state dispatch takes precedence over goto dispatch.
		void	execute_frame_tags(int frame, bool state_only = false, bool goto_only = false)
		// Execute the tags associated with the specified frame.
		// frame is 0-based
		{
			// Keep this (particularly m_as_environment) alive during execution!
			smart_ptr<as_object_interface>	this_ptr(this);

			assert(frame >= 0);
			assert(frame < m_def->get_frame_count());

			const array<execute_tag*>&	playlist = m_def->get_playlist(frame);
			for (int i = 0; i < playlist.size(); i++)
			{
				execute_tag*	e = playlist[i];
				if (state_only)
				{
					e->execute_state(this);
				}
				else if (goto_only)
				{
					e->execute_goto(this);
				}
				else
				{
					e->execute(this);
				}
			}
		}


		/*sprite_instance*/
		void	execute_frame_tags_reverse(int frame)
		// Execute the tags associated with the specified frame, IN REVERSE.
		// I.e. if it's an "add" tag, then we do a "remove" instead.
		// Only relevant to the display-list manipulation tags: add, move, remove, replace.
		//
		// frame is 0-based
		{
			// Keep this (particularly m_as_environment) alive during execution!
			smart_ptr<as_object_interface>	this_ptr(this);

			assert(frame >= 0);
			assert(frame < m_def->get_frame_count());

			const array<execute_tag*>&	playlist = m_def->get_playlist(frame);
			for (int i = 0; i < playlist.size(); i++)
			{
				execute_tag*	e = playlist[i];
				e->execute_state_reverse(this, frame);
			}
		}

		
		/*sprite_instance*/
		execute_tag*	find_previous_replace_or_add_tag(int frame, int depth, int id)
		{
			uint32	depth_id = ((depth & 0x0FFFF) << 16) | (id & 0x0FFFF);

			for (int f = frame - 1; f >= 0; f--)
			{
				const array<execute_tag*>&	playlist = m_def->get_playlist(f);
				for (int i = playlist.size() - 1; i >= 0; i--)
				{
					execute_tag*	e = playlist[i];
					if (e->get_depth_id_of_replace_or_add_tag() == depth_id)
					{
						return e;
					}
				}
			}

			return NULL;
		}


		/*sprite_instance*/
		void	execute_remove_tags(int frame)
		// Execute any remove-object tags associated with the specified frame.
		// frame is 0-based
		{
			assert(frame >= 0);
			assert(frame < m_def->get_frame_count());

			const array<execute_tag*>&	playlist = m_def->get_playlist(frame);
			for (int i = 0; i < playlist.size(); i++)
			{
				execute_tag*	e = playlist[i];
				if (e->is_remove_tag())
				{
					e->execute_state(this);
				}
			}
		}


		/*sprite_instance*/
		void	do_actions()
		// Take care of this frame's actions.
		{
			// Keep m_as_environment alive during any method calls!
			smart_ptr<as_object_interface>	this_ptr(this);

			//  The shipped do_actions (sub_10081480, the loop at
			// 0x100814C0) is a queue DRAIN: while the list is non-empty it takes the FRONT buffer,
			// removes it (sub_10081550 = remove(0)) and only THEN executes it (sub_100961E0 =
			// execute(&m_as_environment)); the storage is released afterwards. Stock 2006 walked
			// the live array by index and cleared it at the end, so a goto applied from INSIDE a
			// running frame script - the label form is immediate, and apply_goto_frame's tail calls
			// do_actions for the target frame's queued actions - re-ran the very buffer that was
			// executing, still at index 0. mainMenu.swf's root frame-40 login script navigated to
			// the main menu (49) through _root.gotoPage and was then run a second time from its
			// first line; its second Profile.loginOfflineAccount was correctly refused (a profile
			// was already current) and the page went back to the account list.
			// With the buffer already off the
			// list, the nested drain sees only the target frame's actions, as shipped.
			while (m_action_list.size() > 0)
			{
				action_buffer*	buf = m_action_list[0];
				m_action_list.remove(0);
				buf->execute(&m_as_environment);
			}
			// SwiffPlayer.dll 0x100814DE clears the action storage.
			m_action_list.clear();
		}


		/*sprite_instance*/
		void	goto_frame(int target_frame_number)
		// Set the sprite state at the specified frame number.
		// 0-based frame numbers!!  (in contrast to ActionScript and Flash MX)
		{
			//  A numeric goto is DEFERRED. The shipped goto_frame (sprite
			// vtable +168, sub_10081310) only appends the target to a per-sprite queue; advance()
			// (0x10081CF0) drains it before its frame pass, applying each entry through
			// apply_goto_frame (vtable +24) with the duplicates right behind it dropped, and runs
			// the pass with a zero delta. The label form (goto_labeled_frame, vtable +28) applies
			// at once. Stock 2006 replayed the timeline inline here.
			m_pending_goto_frames.push_back(target_frame_number);
#if !defined(_MSC_VER)
			if (g_swf_mouse_pass_depth > 0)
			{
				SWF_LIST_TRACE("gotoFrame queued '%s' -> %d", swf_trace_path(this), target_frame_number);
			}
#endif

			// SwiffPlayer.dll 0x10081310 only enqueues; it does not change play state.
		}

		//  The depth -> [placing tag, move tags...] state in effect after
		// frames 0..frame inclusive, as the shipped sub_100832F0 builds it: PLACE and REPLACE start
		// a depth's list, MOVE appends to an existing one (a move with no placement is dropped) and
		// REMOVE leaves an EMPTY list, so a removed depth is distinguishable from an unused one.
		void	build_timeline_state(int frame, hash<int, array<execute_tag*> >* out)
		{
			// SwiffPlayer.dll 0x100832F0 clamps only the upper bound,
			// calling the definition again on that arm; a negative frame visits no tags.
			if (m_def->get_frame_count() <= frame)
			{
				frame = m_def->get_frame_count() - 1;
			}
			for (int f = 0; f <= frame; f++)
			{
				const array<execute_tag*>&	playlist = m_def->get_playlist(f);
				for (int i = 0; i < playlist.size(); i++)
				{
					execute_tag*	e = playlist[i];
					if (e->is_place_tag())
					{
						const int	kind = e->get_place_type();
						const int	depth = e->get_tag_depth();
						if (kind == place_object_2::PLACE || kind == place_object_2::REPLACE)
						{
							array<execute_tag*>	list;
							list.push_back(e);
							out->set(depth, list);
						}
						else if (kind == place_object_2::MOVE)
						{
							array<execute_tag*>	list;
							if (out->get(depth, &list))
							{
								list.push_back(e);
								out->set(depth, list);
							}
						}
					}
					else if (e->is_remove_tag())
					{
						out->set(e->get_tag_depth(), array<execute_tag*>());
					}
				}
			}
		}

		//  The shipped goto (sprite vtable +24, sub_100822A0) is a
		// TIMELINE DIFF, not a tag replay. Both directions build the depth state after the current
		// and after the target frame, mark every referenced child by depth, and walk the target
		// state: a depth with no list (removed), no current placement, or a DIFFERENT placing tag
		// stays marked and its whole target list is queued (it is removed first, then re-placed);
		// the SAME placing tag keeps the child (unmarked) - forward, the moves already applied (the
		// common prefix) are dropped and only the new ones queued; backward, the placing tag's own
		// matrix (only while the child still accepts anim moves; +256 read as get_accept_anim_moves,
		// IDA's is_enabled label is unverified [INFERRED]) and cxform are re-applied and every move
		// after it queued. Every child still marked is removed (remove_display_object(depth)), the
		// queued lists run through execute() (+4), the target frame's playlist runs through
		// execute_goto (+12), then current = target, the time remainder clears, the list is swept,
		// ENTER_FRAME fires and the queued actions run. The play state is never touched here.
		// Stock 2006 replayed every intermediate frame's tags - instantiating, scripting and
		// removing every page a long jump passes: FORGOT PASSWORD jumped the root 9 -> 259 through
		// ten pages, and their scripts reached rows the native client gates - and replayed a
		// backward jump in reverse, re-adding then removing then re-placing the same list twice
		// (RETRIEVE ACCOUNT -> MANAGE ACCOUNTS lost its rows).
		// [CHOSEN, not recovered] S3 bridge: use the native base hash's traversal.
		// [RETIRE-WHEN: swf-core-goto-hash-order] Certify its iteration/insertion
		// policy after wave-3 base recovery and reentrant navigation/menu/reload gates.
		void	apply_goto_frame(int target_frame_number) override
		{
#if !defined(_MSC_VER)
			// [TEMP] c-swf-disconnect: gotos inside a mouse pass, or any that change the list size.
			const int	trace_dl0 = m_display_list.get_character_count();
			const int	trace_from = m_current_frame;
#define SWF_TRACE_GOTO_SWEPT() \
			if (swf_list_trace_on() && (g_swf_mouse_pass_depth > 0 || m_display_list.get_character_count() != trace_dl0)) \
			{ \
				SWF_LIST_TRACE("goto '%s' frame %d -> %d dlist %d -> %d", swf_trace_path(this), trace_from, \
					target_frame_number, trace_dl0, m_display_list.get_character_count()); \
			}
#else
#define SWF_TRACE_GOTO_SWEPT()
#endif

			target_frame_number = iclamp(target_frame_number, 0, m_def->get_frame_count() - 1);
			// [TEMP] RW-88 nav tracer: which clip navigates where, and from what.
			log_error("[goto_frame] '%s' %d -> %d (of %d) dlist=%d\n", swf_trace_path(this),
			          m_current_frame, target_frame_number, m_def->get_frame_count(),
			          m_display_list.get_character_count());

			if (target_frame_number == m_current_frame)
			{
				// SwiffPlayer.dll 0x10082AB1: frame zero is replayed once,
				// including display tags, before taking the common goto tail.
				if (target_frame_number != 0 || m_first_frame_initialized)
				{
					return;
				}
				execute_frame_tags(0);
				m_first_frame_initialized = true;
				m_current_frame = 0;
				m_next_frame = 1;
				m_time_remainder = 0;
				m_update_frame = false;
				m_display_list.update();
				SWF_TRACE_GOTO_SWEPT();
				on_event(event_id::ENTER_FRAME);
				do_actions();
				return;
			}
			const bool	backward = target_frame_number < m_current_frame;

			// Every referenced child, by depth, is a removal candidate until the diff keeps it.
			hash<int, bool>	present;
			for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
			{
				const display_object_info&	dobj = m_display_list.get_display_object(i);
				character*	ch = dobj.m_character.get_ptr();
				if (dobj.m_ref && ch)
				{
					present.set(ch->get_depth(), true);
				}
			}

			hash<int, array<execute_tag*> >	current_state;
			hash<int, array<execute_tag*> >	target_state;
			build_timeline_state(m_current_frame, &current_state);
			build_timeline_state(target_frame_number, &target_state);

			array< array<execute_tag*> >	queued;
			// SwiffPlayer.dll 0x10082381 / 0x100827A7: visit target hash entries directly.
			for (hash<int, array<execute_tag*> >::iterator it = target_state.begin(); it != target_state.end(); ++it)
			{
				const int	depth = it->first;
				array<execute_tag*>	list = it->second;
				if (list.size() == 0)
				{
					continue;	// removed at the target: stays marked, removed below.
				}
				execute_tag*	placing = list[0];
				array<execute_tag*>	current;
				if (!current_state.get(depth, &current) || current.size() == 0 || current[0] != placing)
				{
					queued.push_back(list);	// a new or a different placement: removed, then re-placed.
					continue;
				}

				// The same placement survives.
				present.set(depth, false);
				if (backward)
				{
					character*	ch = m_display_list.get_character_at_depth(depth);
					if (ch)
					{
						place_object_2*	p = static_cast<place_object_2*>(placing);
						if (ch->get_accept_anim_moves() && p->m_has_matrix)
						{
							ch->set_matrix(p->m_matrix);
						}
						if (p->m_has_cxform)
						{
							ch->set_cxform(p->m_color_transform);
						}
					}
					list.remove(0);
				}
				else
				{
					list.remove(0);
					for (int n = 1; n < current.size(); n++)
					{
						if (list.size() == 0 || current[n] != list[0])
						{
							break;
						}
						list.remove(0);
					}
				}
				if (list.size() > 0)
				{
					queued.push_back(list);
				}
			}

			// Remove what the target does not keep.
			for (hash<int, bool>::iterator it = present.begin(); it != present.end(); ++it)
			{
				if (it->second)
				{
					m_display_list.remove_display_object((Uint16) it->first);
				}
			}

			// Place and move what it adds (execute, vtable +4).
			for (int q = 0; q < queued.size(); q++)
			{
				const array<execute_tag*>&	list = queued[q];
				for (int t = 0; t < list.size(); t++)
				{
					list[t]->execute(this);
				}
			}

			// The target frame's own playlist through the goto slot (+12): actions queue, the
			// display tags do nothing, anything else executes.
			{
				// SwiffPlayer.dll 0x100826BC / 0x10082A45: retain the receiver only
				// around execute_goto, after placement execution and before the common tail.
				smart_ptr<as_object_interface>	this_ptr(this);
				const array<execute_tag*>&	playlist = m_def->get_playlist(target_frame_number);
				for (int i = 0; i < playlist.size(); i++)
				{
					playlist[i]->execute_goto(this);
				}
			}

			m_current_frame = target_frame_number;
			m_next_frame = target_frame_number + 1;
			m_time_remainder = 0;
			m_update_frame = false;
			m_display_list.update();
				SWF_TRACE_GOTO_SWEPT();
			on_event(event_id::ENTER_FRAME);
			do_actions();
		}


		// SwiffPlayer.dll 0x1007FED0: ActionScript navigation queues the label.
		void	queue_goto_labeled_frame(const char* label) override
		{
			int target_frame = -1;
			if (m_def->get_labeled_frame(label, &target_frame))
			{
				goto_frame(target_frame);
			}
			else
			{
				log_error("error: movie_impl::goto_labeled_frame('%s') unknown label\n", label);
			}
		}

		bool	goto_labeled_frame(const char* label)
		// Look up the labeled frame, and jump to it.
		{
			int	target_frame = -1;
			if (m_def->get_labeled_frame(label, &target_frame))
			{
				//  applied at once, as the shipped +28 (0x100802E0)
				// dispatches straight to +24 and preserves the play state. ActionScript
				// gotoAndStop/gotoAndPlay use the separate queued-label entry above.
#if !defined(_MSC_VER)
				if (g_swf_mouse_pass_depth > 0)
				{
					SWF_LIST_TRACE("gotoLabel '%s' -> '%s'", swf_trace_path(this), label);
				}
#endif
				apply_goto_frame(target_frame);
				return true;
			}
			else
			{
				// SwiffPlayer.dll 0x1008031F logs every failed lookup.
				log_error("error: movie_impl::goto_labeled_frame('%s') unknown label\n", label);
				return false;
			}
		}

		
		/*sprite_instance*/
		void	display()
		{
			if (get_visible() == false)
			{
				// We're invisible, so don't display!
				return;
			}

			m_display_list.display();

			// SwiffPlayer.dll 0x1007FCA0 ends after displaying the list.
		}

		/*sprite_instance*/
		character*	add_display_object(
			Uint16 character_id,
			const char* name,
			const array<swf_event*>& event_handlers,
			Uint16 depth,
			bool replace_if_depth_is_occupied,
			const cxform& color_transform,
			const matrix& matrix,
			float ratio,
			Uint16 clip_depth)
		// Add an object to the display list.
		{
			assert(m_def != NULL);

			character_def*	cdef = m_def->get_character_def(character_id);
			if (cdef == NULL)
			{
				log_error("sprite::add_display_object(): unknown cid = %d\n", character_id);
				return NULL;
			}

			// SwiffPlayer.dll 0x100837E0 keeps an exact referenced occupant,
			// without applying the new placement's transforms or handlers.
			const int index = m_display_list.find_display_index(depth);
			if (index >= 0 && index < m_display_list.get_character_count())
			{
				const display_object_info existing = m_display_list.get_display_object(index);
				character* old = existing.m_character.get_ptr();
				if (existing.m_ref && old->get_depth() == depth && old->get_id() == character_id
					&& (name == NULL ? old->get_name().length() == 0
						: strcmp(old->get_name().c_str(), name) == 0))
				{
					return old;
				}
			}

			assert(cdef);
			smart_ptr<character>	ch = cdef->create_character_instance(this, character_id);
			assert(ch != NULL);
			if (name != NULL && name[0] != 0)
			{
				ch->set_name(name);
			}

			// Attach event handlers (if any).
			{for (int i = 0, n = event_handlers.size(); i < n; i++)
			{
				event_handlers[i]->attach_to(ch.get_ptr());
			}}

			m_display_list.add_display_object(
				ch.get_ptr(),
				depth,
				replace_if_depth_is_occupied,
				color_transform,
				matrix,
				ratio,
				clip_depth);

			assert(ch == NULL || ch->get_ref_count() > 1);
			return ch.get_ptr();
		}

		// Instantiates a runtime definition without a dictionary lookup.
		// SwiffPlayer.dll 0x1007BA80; the native helper supplies default transforms
		// and keeps the definition alive through deferred display-list removal.
		character*	add_display_object_def(
			character_def* cdef,
			const char* name,
			Uint16 depth)
		{
			// SwiffPlayer.dll 0x1007BA80 checks the definition before the factory.
			if (cdef == NULL)
			{
				// Retail's diagnostic has an unmatched conversion; retain its format.
				log_error("sprite::add_display_object(): cdef = NULL%d\n");
				return NULL;
			}
			assert(cdef);
			smart_ptr<character>	ch = cdef->create_character_instance(this, 0);
			if (ch == NULL)
			{
				return NULL;
			}
			// OWNERSHIP: a runtime-built definition (the drawing canvas, a loadMovie image shape) has
			// no character table behind it, and display-list removal is DEFERRED to the next sweep:
			// the child outlives the sprite's m_canvas/m_image handle by up to a frame. The child
			// therefore holds its own definition (harness Z / retail Q: a button's stale canvas child
			// read a freed shape definition's vtable inside get_width - the jump to 0x3F000000).
			ch->hold_definition(cdef);
			if (name != NULL && name[0] != 0)
			{
				ch->set_name(name);
			}
			m_display_list.add_display_object(
				ch.get_ptr(),
				depth,
				true,
				cxform::identity,
				matrix::identity,
				1.0f,
				0);
			return ch.get_ptr();
		}

		// the highest depth in use, -1 when empty - for getNextHighestDepth.
		int	get_highest_depth_in_use()
		{
			int	highest = -1;
			for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
			{
				character*	ch = m_display_list.get_character(i);
				if (ch && ch->get_depth() > highest)
				{
					highest = ch->get_depth();
				}
			}
			return highest;
		}

		// SwiffPlayer.dll 0x1006F960 / 0x1006F9F0 / 0x1007BBF0:
		// postincrement without a depth scan.
		int	take_next_highest_depth()
		{
			return m_next_highest_depth++;
		}

		// SwiffPlayer.dll 0x1007C0D0 resolves the named canvas through member
		// lookup on every call, and creates it at depth zero only when absent.
		shape_character_def*	get_canvas_def()
		{
			if (this == NULL) return NULL;
			as_value canvas;
			if (!get_member("_____defaultShape", &canvas))
			{
				m_canvas = new shape_character_def;
				m_canvas->set_character_id(0);
				add_display_object_def(m_canvas.get_ptr(), "_____defaultShape", 0);
			}
			else
			{
				// [CHOSEN, not recovered] S3 bridge: the native generic
				// character carries the definition directly instead of retail +0xC8.
				generic_character* ch = static_cast<generic_character*>(canvas.to_object()->to_movie());
				m_canvas = static_cast<shape_character_def*>(ch->m_def.get_ptr());
			}
			return m_canvas.get_ptr();
		}

		// remove every child - the first thing the shipped loadMovie does.
		void	remove_all_display_objects()
		{
			// SwiffPlayer.dll 0x1007BC07 walks the live referenced entries.
			for (int i = 0; i < m_display_list.get_character_count(); i++)
			{
				character*	ch = m_display_list.get_character(i);
				if (ch)
				{
					m_display_list.remove_display_object((Uint16) ch->get_depth());
				}
			}
			// [CHOSEN, not recovered] S3 bridge: the native canvas owner
			// drops its handle after removal; each child still owns its definition.
			m_canvas = NULL;
		}


		/*sprite_instance*/
		// SwiffPlayer.dll 0x1007FCC0.
		void	move_display_object(
			Uint16 depth,
			bool use_cxform,
			const cxform& color_xform,
			bool use_matrix,
			const matrix& mat,
			float ratio,
			Uint16 clip_depth)
		// Updates the transform properties of the object at
		// the specified depth.
		{
			m_display_list.move_display_object(depth, use_cxform, color_xform, use_matrix, mat, ratio, clip_depth);
		}


		/*sprite_instance*/
		// SwiffPlayer.dll 0x10080490.
		void	replace_display_object(
			Uint16 character_id,
			const char* name,
			Uint16 depth,
			bool use_cxform,
			const cxform& color_transform,
			bool use_matrix,
			const matrix& mat,
			float ratio,
			Uint16 clip_depth)
		{
			assert(m_def != NULL);

			character_def*	cdef = m_def->get_character_def(character_id);
			if (cdef == NULL)
			{
				log_error("sprite::replace_display_object(): unknown cid = %d\n", character_id);
				return;
			}
			assert(cdef);

			smart_ptr<character>	ch = cdef->create_character_instance(this, character_id);
			assert(ch != NULL);

			if (name != NULL && name[0] != 0)
			{
				ch->set_name(name);
			}

			m_display_list.replace_display_object(
				ch.get_ptr(),
				depth,
				use_cxform,
				color_transform,
				use_matrix,
				mat,
				ratio,
				clip_depth);
		}


		/*sprite_instance*/
		// SwiffPlayer.dll 0x10080540.
		void	replace_display_object(
			character* ch,
			const char* name,
			Uint16 depth,
			bool use_cxform,
			const cxform& color_transform,
			bool use_matrix,
			const matrix& mat,
			float ratio,
			Uint16 clip_depth)
		{

			assert(ch != NULL);

			if (name != NULL && name[0] != 0)
			{
				ch->set_name(name);
			}

			m_display_list.replace_display_object(
				ch,
				depth,
				use_cxform,
				color_transform,
				use_matrix,
				mat,
				ratio,
				clip_depth);
		}


		/*sprite_instance*/
		// SwiffPlayer.dll 0x1007FCD0.
		void	remove_display_object(Uint16 depth)
		// Remove the object at the specified depth.
		// If id != -1, then only remove the object at depth with matching id.
		{
			m_display_list.remove_display_object(depth);
		}


		// SwiffPlayer.dll 0x10081340.
		void	add_action_buffer(action_buffer* a)
		// Add the given action buffer to the list of action
		// buffers to be processed at the end of the next
		// frame advance.
		{
			m_action_list.push_back(a);
		}


		/*sprite_instance*/
		int	get_id_at_depth(int depth)
		// For debugging -- return the id of the character at the specified depth.
		// Return -1 if nobody's home.
		{
			int	index = m_display_list.get_display_index(depth);
			if (index == -1)
			{
				return -1;
			}

			character*	ch = m_display_list.get_display_object(index).m_character.get_ptr();

			return ch->get_id();
		}


		//
		// ActionScript support
		//


		/* sprite_instance */
		// SwiffPlayer.dll 0x10081F50 has no null-argument refusal path.
		virtual void	set_variable(const char* path_to_var, const char* new_value)
		{
			assert(m_parent == NULL);	// should only be called on the root movie.

			array<with_stack_entry>	empty_with_stack;
			tu_string	path(path_to_var);
			as_value	val(new_value);

			m_as_environment.set_variable(path, val, empty_with_stack);
		}

		/* sprite_instance */
		// SwiffPlayer.dll 0x100820C0 refuses only a null wide value.
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value)
		{
			if (new_value == NULL)
			{
				log_error("error: NULL passed to set_variable('%s', NULL)\n", path_to_var);
				return;
			}

			assert(m_parent == NULL);	// should only be called on the root movie.

			array<with_stack_entry>	empty_with_stack;
			tu_string	path(path_to_var);
			as_value	val(new_value);

			m_as_environment.set_variable(path, val, empty_with_stack);
		}

		/* sprite_instance */
		// SwiffPlayer.dll 0x100821D0 returns the shared static value's string storage.
		virtual const char*	get_variable(const char* path_to_var) const
		{
			assert(m_parent == NULL);	// should only be called on the root movie.

			array<with_stack_entry>	empty_with_stack;
			tu_string	path(path_to_var);

			// NOTE: this is static so that the string
			// value won't go away after we return!!!
			// It'll go away during the next call to this
			// function though!!!  NOT THREAD SAFE!
			static as_value	val;

			val = m_as_environment.get_variable(path, empty_with_stack);

			return val.to_string();	// ack!
		}

		
		/* sprite_instance */
		virtual void	set_member(const tu_stringi& name, const as_value& val)
		// Assign an accessor, display property, matching text field or script member.
		{
			// SwiffPlayer.dll 0x10083A20, secondary interface 0x100E0C3C +0x20.
			// SwiffPlayer.dll 0x10083A49 tries the script object's setter
			// before handler detection or built-ins. [CHOSEN, not recovered]
			// S3 bridge: adjust the native accessor receiver to the sprite.
			if (m_script_object != NULL)
			{
				if (m_script_object->set_property(name, val, this))
				{
					return;
				}
			}
			// The handler-name table is initialized only after the prefix test;
			// assigning even an undefined value makes mouse participation sticky.
			if (!m_mouse_enabled)
			{
				const char*	s = name.c_str();
				if (s[0] == 'o' && s[1] == 'n' && s[2] >= 'A' && s[2] <= 'Z')
				{
					static string_hash<bool> mouse_handlers;
					static bool initialized = false;
					if (!initialized)
					{
						static const char* const names[] = {
							"onDragOut", "onDragOver", "onMouseDown", "onMouseMove", "onMouseUp", "onPress",
							"onDoublePress", "onRelease", "onReleaseOutside", "onRollOut", "onRollOver",
						};
						for (int i = 0; i < ARRAYSIZE(names); ++i)
							mouse_handlers.set(names[i], true);
						initialized = true;
					}
					if (mouse_handlers.get(name.c_str(), NULL)) m_mouse_enabled = true;
				}
			}
			// SwiffPlayer.dll 0x10083F19 controls forced per-pass playback.
			if (strcmp(name.c_str(), "maxFPS") == 0)
			{
				m_force_frame_update = val.to_bool();
				return;
			}
			as_standard_member	std_member = sprite_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_X:
				//if (name == "_x")
			{
				matrix	m = get_matrix();
				// SwiffPlayer.dll 0x10083FA5: convert before the float store.
				m.m_[0][2] = bf2x87::store(bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(20.0f)));
				set_matrix(m);

				m_accept_anim_moves = false;

				return;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();
				// SwiffPlayer.dll 0x1008401C.
				m.m_[1][2] = bf2x87::store(bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(20.0f)));
				set_matrix(m);

				m_accept_anim_moves = false;

				return;
			}
			case M_XSCALE:
				//else if (name == "_xscale")
			{
				matrix	m = get_matrix();

				// Decompose matrix and insert the desired value.
				// SwiffPlayer.dll 0x10084094 multiplies by the stored float reciprocal.
				float	x_scale = bf2x87::store(bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(0.01f)));
				float	y_scale = m.get_y_scale();
				float	rotation = m.get_rotation();
				m.set_scale_rotation(x_scale, y_scale, rotation);

				set_matrix(m);
				m_accept_anim_moves = false;
				return;
			}
			case M_YSCALE:
				//else if (name == "_yscale")
			{
				matrix	m = get_matrix();

				// Decompose matrix and insert the desired value.
				float	x_scale = m.get_x_scale();
				// SwiffPlayer.dll 0x1008410E.
				float	y_scale = bf2x87::store(bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(0.01f)));
				float	rotation = m.get_rotation();
				m.set_scale_rotation(x_scale, y_scale, rotation);

				set_matrix(m);
				m_accept_anim_moves = false;
				return;
			}
			case M_ALPHA:
				//else if (name == "_alpha")
			{
				// Set alpha modulate, in percent.
				cxform	cx = get_cxform();
				// SwiffPlayer.dll 0x100841D4.
				cx.m_[3][0] = bf2x87::store(bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(0.01f)));
				set_cxform(cx);
				m_accept_anim_moves = false;
				return;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				const bool visible = val.to_bool();
				log_error("[_visible] '%s' = %d\n", swf_trace_path(this), visible ? 1 : 0);  // [TEMP] RW-88
				set_visible(visible);
				m_accept_anim_moves = false;
				return;
			}
			case M_WIDTH:
				//else if (name == "_width")
			{
				// SwiffPlayer.dll 0x10084261-0x100842F2 repeats conversion and
				// extent dispatch; only the division's numerator narrows to float.
				matrix m = get_matrix();
				if (val.to_number() == 0.0 || get_width() == 0.0f) m.m_[0][0] = 0.0f;
				else
				{
					const float numerator = (float)val.to_number();
					m.m_[0][0] = bf2x87::store(bf2x87::div(bf2x87::Real(numerator), bf2x87::Real(get_width())));
				}
				set_matrix(m);
				m_accept_anim_moves = false;
				return;
			}
			case M_HEIGHT:
				//else if (name == "_height")
			{
				// SwiffPlayer.dll 0x10084320-0x100843A8.
				matrix m = get_matrix();
				if (val.to_number() == 0.0 || get_height() == 0.0f) m.m_[1][1] = 0.0f;
				else
				{
					const float numerator = (float)val.to_number();
					m.m_[1][1] = bf2x87::store(bf2x87::div(bf2x87::Real(numerator), bf2x87::Real(get_height())));
				}
				set_matrix(m);
				m_accept_anim_moves = false;
				return;
			}
			case M_ROTATION:
				//else if (name == "_rotation")
			{
				matrix	m = get_matrix();

				// Decompose matrix and insert the desired value.
				float	x_scale = m.get_x_scale();
				float	y_scale = m.get_y_scale();
				// SwiffPlayer.dll 0x10084185 uses the float radians-per-degree constant.
				float	rotation = bf2x87::store(bf2x87::mul(bf2x87::Real(val.to_number()), bf2x87::Real(0.017453292f)));
				m.set_scale_rotation(x_scale, y_scale, rotation);

				set_matrix(m);
				m_accept_anim_moves = false;
				return;
			}
			case M_HIGHQUALITY:
				//else if (name == "_highquality")
			{
				// @@ global { 0, 1, 2 }
//				// Whether we're in high quality mode or not.
//				val->set(true);
				return;
			}
			case M_FOCUSRECT:
				//else if (name == "_focusrect")
			{
//				// Is a yellow rectangle visible around a focused movie clip (?)
//				val->set(false);
				return;
			}
			case M_SOUNDBUFTIME:
				//else if (name == "_soundbuftime")
			{
				// @@ global
//				// Number of seconds before sound starts to stream.
//				val->set(0.0);
				return;
			}
			}	// end switch

			// SwiffPlayer.dll 0x10084421: qword truncation, low dword store, then
			// the recursive tab-order dispatch. It does not disable animation moves.
			if (strcmp(name.c_str(), "tabIndex") == 0)
			{
				m_tab_index = bf2x87::lowWord(bf2x87::toInt64Truncate(bf2x87::Real(val.to_number())));
				set_tab_index(m_tab_index);
				return;
			}

			// Not a built-in property.  See if we have a
			// matching edit_text character in our display
			// list.
			bool	text_val = val.get_type() == as_value::STRING
				|| val.get_type() == as_value::NUMBER;
			if (text_val)
			{
				// SwiffPlayer.dll 0x1008447D converts once before reading the list count.
				const char* text = val.to_string();
				bool	success = false;
				for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
				{
					character*	ch = m_display_list.get_character(i);
					// SwiffPlayer.dll 0x100844B0 compares text names exactly.
					if (ch && strcmp(name.c_str(), ch->get_text_name()) == 0)
					{
						ch->set_text_value(text);
						success = true;
					}
				}
				if (success) return;
			}

			// SwiffPlayer.dll 0x100844FD writes into the private script object
			// when present; only an unbound sprite writes into its environment.
			if (m_script_object != NULL) m_script_object->set_member(name, val);
			else m_as_environment.set_member(name, val);
		}


		/* sprite_instance */
		virtual bool	get_member(const tu_stringi& name, as_value* val)
		// Set *val to the value of the named member and
		// return true, if we have the named member.
		// Otherwise leave *val alone and return false.
		{
			// SwiffPlayer.dll 0x10080590, secondary interface 0x100E0C3C +0x24.
			// SwiffPlayer.dll 0x10080590 tests these names before object dispatch.
			if (strcmp(name.c_str(), "prototype") == 0)
			{
				// SwiffPlayer.dll 0x100805C8 retains a temporary before output cleanup.
				*val = as_value(m_script_object.get_ptr());
				return true;
			}
			if (strcmp(name.c_str(), "super") == 0)
			{
				// SwiffPlayer.dll 0x10080625 uses the same temporary-value assignment.
				*val = as_value(m_as_environment.m_super_ctor);
				return true;
			}
			// 0x10080676 calls the private object's member virtual once.
			// Its accessor receiver is the private object, unlike the setter's
			// explicit sprite receiver. The ordinary lookup also publishes super.
			if (m_script_object != NULL && m_script_object->get_member(name, val)) return true;
			as_standard_member	std_member = sprite_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_X:
				//if (name == "_x")
			{
				matrix	m = get_matrix();
				// SwiffPlayer.dll 0x100806D5 publishes directly as double.
				val->set_double(double(m.m_[0][2]) * double(0.05f));
				return true;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();
				// SwiffPlayer.dll 0x10080737 has no intermediate float store for Y.
				val->set_double(double(m.m_[1][2]) * double(0.05f));
				return true;
			}
			case M_XSCALE:
				//else if (name == "_xscale")
			{
				matrix m = get_matrix();	// @@ or get_world_matrix()?  Test this.
				float xscale = m.get_x_scale();
				val->set_double(double(xscale) * 100.0);
				return true;
			}
			case M_YSCALE:
				//else if (name == "_yscale")
			{
				matrix m = get_matrix();	// @@ or get_world_matrix()?  Test this.
				float yscale = m.get_y_scale();
				val->set_double(double(yscale) * 100.0);
				return true;
			}
			case M_CURRENTFRAME:
				//else if (name == "_currentframe")
			{
				val->set_int(m_current_frame + 1);
				return true;
			}
			case M_TOTALFRAMES:
				//else if (name == "_totalframes")
			{
				// number of frames.  Read only.
				val->set_int(m_def->get_frame_count());
				return true;
			}
			case M_ALPHA:
				//else if (name == "_alpha")
			{
				// Alpha units are in percent.
				val->set_double(double(get_cxform().m_[3][0]) * 100.0);
				return true;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				val->set_bool(get_visible());
				return true;
			}
			case M_WIDTH:
				//else if (name == "_width")
			{
				// SwiffPlayer.dll 0x100808DD snapshots the local matrix before
				// the rectangle-based pixel extent and publishes the product as double.
				const matrix m = get_matrix();
				val->set_double(bf2x87::storeDouble(bf2x87::mul(bf2x87::Real(get_width()), bf2x87::Real(m.m_[0][0]))));
				return true;
			}
			case M_HEIGHT:
				//else if (name == "_height")
			{
				// SwiffPlayer.dll 0x10080936, same ordering for height.
				const matrix m = get_matrix();
				val->set_double(bf2x87::storeDouble(bf2x87::mul(bf2x87::Real(get_height()), bf2x87::Real(m.m_[1][1]))));
				return true;
			}
			case M_ROTATION:
				//else if (name == "_rotation")
			{
				// Verified against Macromedia player using samples/test_rotation.swf
				float	angle = get_matrix().get_rotation();

				// Result is CLOCKWISE DEGREES, [-180,180]
				// SwiffPlayer.dll 0x1008099D publishes the product directly as double.
				val->set_double(double(angle) * double(57.29578f));
				return true;
			}
			case M_TARGET:
				//else if (name == "_target")
			{
				// Full path to this object; e.g. "/_level0/sprite1/sprite2/ourSprite"
				val->set_string("/_root");
				return true;
			}
			case M_FRAMESLOADED:
				//else if (name == "_framesloaded")
			{
				val->set_int(m_def->get_frame_count());
				return true;
			}
			case M_NAME:
				//else if (name == "_name")
			{
				val->set_tu_string(get_name());
				return true;
			}
			case M_DROPTARGET:
				//else if (name == "_droptarget")
			{
				// Absolute path in slash syntax where we were last dropped (?)
				// @@ TODO
				val->set_string("/_root");
				return true;
			}
			case M_URL:
				//else if (name == "_url")
			{
				// our URL.
				val->set_string("gameswf");
				return true;
			}
			case M_HIGHQUALITY:
				//else if (name == "_highquality")
			{
				// Whether we're in high quality mode or not.
				val->set_bool(true);
				return true;
			}
			case M_FOCUSRECT:
				//else if (name == "_focusrect")
			{
				// Is a yellow rectangle visible around a focused movie clip (?)
				val->set_bool(false);
				return true;
			}
			case M_SOUNDBUFTIME:
				//else if (name == "_soundbuftime")
			{
				// Number of seconds before sound starts to stream.
				val->set_double(0.0);
				return true;
			}
			case M_XMOUSE:
				//else if (name == "_xmouse")
			{
				// Local coord of mouse IN PIXELS.
				int	x, y, buttons;
				assert(m_root);
				m_root->get_mouse_state(&x, &y, &buttons);

				matrix	m = get_world_matrix();

				// SwiffPlayer.dll 0x10080B9F..0x10080BCD converts the integer
				// coordinates before multiplying, then stores each result as float.
				point	a(
					bf2x87::store(bf2x87::mul(bf2x87::Real(x), bf2x87::Real(20.0f))),
					bf2x87::store(bf2x87::mul(bf2x87::Real(y), bf2x87::Real(20.0f))));
				point	b;
				
				m.transform_by_inverse(&b, a);

				// 0x10080BD5 -> 0x10080737 multiplies by the float reciprocal
				// and publishes directly as double, without a float result store.
				val->set_double(double(b.m_x) * double(0.05f));
				return true;
			}
			case M_YMOUSE:
				//else if (name == "_ymouse")
			{
				// Local coord of mouse IN PIXELS.
				int	x, y, buttons;
				assert(m_root);
				m_root->get_mouse_state(&x, &y, &buttons);

				matrix	m = get_world_matrix();

				// SwiffPlayer.dll 0x10080C21..0x10080C4F, as for _xmouse.
				point	a(
					bf2x87::store(bf2x87::mul(bf2x87::Real(x), bf2x87::Real(20.0f))),
					bf2x87::store(bf2x87::mul(bf2x87::Real(y), bf2x87::Real(20.0f))));
				point	b;
				
				m.transform_by_inverse(&b, a);

				// 0x10080C57 -> 0x10080737 has the same double publication.
				val->set_double(double(b.m_y) * double(0.05f));
				return true;
			}
			case M_PARENT:
			{
				// SwiffPlayer.dll 0x10080CB7 jumps to the null-OBJECT result at 0x10080D75.
				val->set_as_object_interface(static_cast<as_object_interface*>(m_parent));
				return true;
			}
			}	// end switch

			// SwiffPlayer.dll 0x10080C79: absent tab order leaves the output alone.
			if (strcmp(name.c_str(), "tabIndex") == 0)
			{
				if (m_tab_index < 0) return false;
				val->set_int(m_tab_index);
				return true;
			}

			// SwiffPlayer.dll 0x10080CEE returns the character's persistent Array.
			if (strcmp(name.c_str(), "arguments") == 0)
			{
				val->set_as_object_interface(m_arguments.get_ptr());
				return true;
			}

			if (strcmp(name.c_str(), "_global") == 0)
			{
				val->set_as_object_interface(get_global_object());
				return true;
			}
			if (strcmp(name.c_str(), "_root") == 0)
			{
				// [CHOSEN, not recovered] S3 bridge for retail's global root.
				val->set_as_object_interface(get_root_movie());
				return true;
			}

			// Not a built-in property.  Check items on our
			// display list.
			// SwiffPlayer.dll 0x10080590 compares child names case-sensitively.
			character*	ch = m_display_list.get_character_by_name(tu_string(name.c_str()));
			if (ch)
			{
				// Found object.
				val->set_as_object_interface(static_cast<as_object_interface*>(ch));
				return true;
			}

			// SwiffPlayer.dll 0x10080E1B: environment lookup follows child lookup.
			if (m_as_environment.get_member(name, val)) return true;
			// Try static builtin functions.
			assert(s_sprite_builtins);
			if (s_sprite_builtins->get_member(name, val))
			{
				return true;
			}

			return false;
		}


		/* sprite_instance */
		// SwiffPlayer.dll 0x10080EA0. The per-tree root is the existing
		// [CHOSEN, not recovered] S3 bridge for retail's global root.
		virtual movie*	get_relative_target(const tu_string& name)
		// Find the movie which is one degree removed from us,
		// given the relative pathname.
		//
		// If the pathname is "..", then return our parent.
		// If the pathname is ".", then return ourself.  If
		// the pathname is "_level0" or "_root", then return
		// the root movie.
		//
		// Otherwise, the name should refer to one our our
		// named characters, so we return it.
		//
		// NOTE: In ActionScript 2.0, top level names (like
		// "_root" and "_level0") are CASE SENSITIVE.
		// Character names in a display list are CASE
		// SENSITIVE. Member names are CASE INSENSITIVE.  Gah.
		//
		// In ActionScript 1.0, everything seems to be CASE
		// INSENSITIVE.
		{
			if (name == "." || name == "this")
			{
				return this;
			}
			else if (name == "..")
			{
				return get_parent();
			}
			else if (name == "_level0"
				 || name == "_root")
			{
				return m_root->m_movie.get_ptr();
			}

			// See if we have a match on the display list.
			return m_display_list.get_character_by_name(name);
		}


		/* sprite_instance */
		// SwiffPlayer.dll 0x10081370 drains all pending actions after each tag,
		// including a non-action tag, before inspecting the next playlist entry.
		virtual void	call_frame_actions(const as_value& frame_spec)
		// Execute the actions for the specified frame.  The
		// frame_spec could be an integer or a string.
		{
			int	frame_number = -1;

			// Figure out what frame to call.
			if (frame_spec.get_type() == as_value::STRING)
			{
				if (m_def->get_labeled_frame(frame_spec.to_string(), &frame_number) == false)
				{
					// Try converting to integer.
					frame_number = bf2x87::toIntTruncate(frame_spec.to_number());
				}
			}
			else
			{
				// convert from 1-based to 0-based
				const Sint32 converted = bf2x87::toIntTruncate(frame_spec.to_number());
				frame_number = bf2x87::lowWord(static_cast<std::int64_t>(converted) - 1);
			}

			if (frame_number < 0 || frame_number >= m_def->get_frame_count())
			{
				// No dice.
				log_error("error: call_frame('%s') -- unknown frame\n", frame_spec.to_string());
				return;
			}

			// Execute the actions.
			const array<execute_tag*>&	playlist = m_def->get_playlist(frame_number);
			for (int i = 0; i < playlist.size(); i++)
			{
				execute_tag*	e = playlist[i];
				if (e->is_action_tag())
				{
					e->execute(this);
				}
				do_actions();
			}
		}


		/* sprite_instance */
		virtual void	set_drag_state(const drag_state& st)
		{
			// SwiffPlayer.dll 0x1007FCE0 copies the completed record.
			// [CHOSEN, not recovered] S3 bridge: use the native per-tree root.
			m_root->m_drag_state = st;
		}

		/* sprite_instance */
		virtual void	stop_drag()
		{
			assert(m_parent == NULL);	// we must be the root movie!!!
			
			m_root->stop_drag();
		}


		/* sprite_instance */
		virtual void	get_drag_state(drag_state* st)
		{
			*st = m_root->m_drag_state;
		}


		// SwiffPlayer.dll 0x100815A0; dictionary placement copies transform,
		// ratio and clip depth, with no cloned script state or event handlers.
		void	clone_display_object(const tu_string& name, const tu_string& newname, Uint16 depth)
		// Duplicate the object with the specified name and add it with a new name 
		// at a new depth.
		{
			character* ch = m_display_list.get_character_by_name(name);
			if (ch)
			{
				array<swf_event*>	dummy_event_handlers;

				add_display_object(
					ch->get_id(),
					newname.c_str(),
					dummy_event_handlers,
					depth,
					true,	// replace if depth is occupied
					ch->get_cxform(),
					ch->get_matrix(),
					ch->get_ratio(),
					ch->get_clip_depth());
				// @@ TODO need to duplicate ch's event handlers, and presumably other members?
				// Probably should make a character::clone() function to handle this.
			}
		}


		// SwiffPlayer.dll 0x1007FD30 performs an exact-name lookup then depth removal.
		void	remove_display_object(const tu_string& name)
		// Remove the object with the specified name.
		{
			character* ch = m_display_list.get_character_by_name(name);
			if (ch)
			{
				// @@ TODO: should only remove movies that were created via clone_display_object --
				// apparently original movies, placed by anim events, are immune to this.
				remove_display_object(ch->get_depth());
			}
		}

		
		/* sprite_instance */
		virtual bool	on_event(event_id id)
		// Dispatch event handler(s), if any.
		{
			// Keep m_as_environment alive during any method calls!
			smart_ptr<as_object_interface>	this_ptr(this);

			bool called = false;
			// SwiffPlayer.dll 0x10081620's
			// temporary action verbosity for mouse down/up, double press, press/release.
			// Event IDs follow the retail event-name table.
			const bool enable_verbose = s_verbose_mouse_events && !s_verbose_action
				&& (id == event_id::MOUSE_DOWN || id == event_id::MOUSE_UP
					|| id == event_id::DOUBLE_PRESS || id == event_id::PRESS || id == event_id::RELEASE);
			if (enable_verbose) s_verbose_action = true;
#if !defined(_MSC_VER)
			// [TEMP] c-swf-disconnect: the events a mouse pass fires (press, release, roll over/out).
			if (g_swf_mouse_pass_depth > 0 && swf_list_trace_on())
			{
				SWF_LIST_TRACE("event %d/%s on '%s' dlist=%d", (int) id.m_id, id.get_function_name().c_str(),
					swf_trace_path(this), m_display_list.get_character_count());
			}
#endif

			//  sub_10081620: the clip-event handler when there is one, ELSE
			// the member of that name, and only a defined one. Stock 2006 called both when both existed.
			{
				as_value	method;
				if (get_event_handler(id, &method))
				{
					call_method0(method, &m_as_environment, this);
					called = true;
				}
			}
			if (!called)
			{
				// In ActionScript 2.0, event method names are CASE SENSITIVE.
				// In ActionScript 1.0, event method names are CASE INSENSITIVE.
				const tu_stringi&	method_name = id.get_function_name().to_tu_stringi();
				if (method_name.length() > 0)
				{
					as_value	method;
					if (get_member(method_name, &method) && method.get_type() != as_value::UNDEFINED)
					{
						call_method0(method, &m_as_environment, this);
						called = true;
					}
				}
			}
#if !defined(_MSC_VER)
			if (called && g_swf_mouse_pass_depth > 0 && swf_list_trace_on())
			{
				SWF_LIST_TRACE("event %d/%s done on '%s' dlist=%d", (int) id.m_id, id.get_function_name().c_str(),
					swf_trace_path(this), m_display_list.get_character_count());
			}
#endif
			// SwiffPlayer.dll 0x10081808 restores verbosity before releasing this_ptr.
			if (enable_verbose) s_verbose_action = false;
			return called;
		}


		/*sprite_instance*/
		virtual void	on_event_load()
		// Run definition initialization and frame-zero placement, then bind a class.
		{
			// SwiffPlayer.dll 0x10081870 executes pending definition-wide
			// init actions before frame zero and clears each bit after execution.
			const array<execute_tag*>* init_actions = m_def->get_init_actions(0);
			for (int i = 0; i < init_actions->size(); ++i)
			{
				if (m_root->m_def->m_init_action_pending[i])
				{
					execute_init_action((*init_actions)[i], &m_as_environment);
					m_root->m_def->m_init_action_pending[i] = false;
				}
			}
			execute_frame_tags(0);
			// SwiffPlayer.dll 0x10081870 discards frame-zero actions here;
			// advance replays the complete playlist after the parent's actions.
			m_action_list.clear();
			// SwiffPlayer.dll 0x10081870 sends CONSTRUCT only when a sprite other
			// than the published global root has a class prototype. It sends
			// neither INITIALIZE nor LOAD.
			// 0x1008191D captures the registered function once. CONSTRUCT can
			// replace the definition binding; the subsequent call still uses this one.
			// [CHOSEN, not recovered] S3 bridge: hold the native function alive
			// across callbacks, since native definitions own their class bindings.
			smart_ptr<as_as_function> cls;
			// 0x10081911 compares directly with the global publication, even
			// when another movie has replaced this sprite's own tree root.
			if (this != get_global_root_movie())
			{
				sprite_definition* sprite_def = sprite_definition_of(m_def.get_ptr());
				cls = sprite_def ? static_cast<as_as_function*>(sprite_def->m_registered_class) : NULL;
			}
			if (cls != NULL && cls->m_properties->m_instance_prototype != NULL)
			{
				bind_class_prototype(cls.get_ptr());
				on_event(event_id::CONSTRUCT);
				const as_value constructor(cls.get_ptr());
				call_method0(constructor, &m_as_environment, this);
			}
		}

		// Read the current prototype's constructor without script-visible lookup.
		as_as_function* class_prototype_constructor(as_as_function* cls)
		{
			// SwiffPlayer.dll 0x10081A12 / 0x10081A7E read concrete fields,
			// without a script-visible "prototype" or "__constructor__" lookup.
			as_object* class_proto = cls->m_properties->m_instance_prototype->cast_to_as_object();
			// [CHOSEN, not recovered] A movie clip as class prototype supplies no constructor.
			// Retail reads the field at the clip's secondary-interface offset, unrelated movie
			// state, and publishes it as __constructor__ and the super target; only super() would
			// use it. Retail 1.50 binds the instance, runs the registered class once and links the
			// clip as its __proto__ (probe v2 tests 4, 7, 8 green; no fault).
			if (class_proto == NULL) return NULL;
			return class_proto->m_constructor.get_ptr();
		}

		void	bind_class_prototype(as_as_function* cls)
		{
			// SwiffPlayer.dll 0x10081870 owns an instance bag, links its prototype
			// and records its movie owner before running construct/constructor.
			m_script_object = new as_object;
			m_script_object->set_member("_____owner", as_value(this));
			m_script_object->set_member("__proto__", as_value(cls->m_properties->m_instance_prototype));
			m_script_object->set_member("__constructor__",
				as_value(class_prototype_constructor(cls)));
			// SwiffPlayer.dll 0x10081A84 reads the prototype again after the
			// member write, then publishes its borrowed constructor before CONSTRUCT.
			set_call_super(class_prototype_constructor(cls));
		}

		// SwiffPlayer.dll 0x10080390 / 0x10080330 retain the receiver through
		// the parser and drop that reference before returning its shared text.
		const char* call_method(const Uint16* expression) override
		{
			smart_ptr<as_object_interface> receiver(this);
			return call_method_expression(&m_as_environment, this, expression);
		}
		const char* call_method(const char* expression) override
		{
			smart_ptr<as_object_interface> receiver(this);
			return call_method_expression(&m_as_environment, this, expression);
		}

		/* sprite_instance */
	};


	// SwiffPlayer.dll 0x100737A0; the native root always owns a sprite instance.
	void movie_root::goto_frame(int target_frame_number)
	{
		static_cast<sprite_instance*>(m_movie.get_ptr())->apply_goto_frame(target_frame_number);
	}

	// SwiffPlayer.dll 0x100849A0; the native factory returns a borrowed raw
	// pointer instead of retail's smart_ptr result [CHOSEN, not recovered] S3 bridge.
	character*	sprite_definition::create_character_instance(movie* parent, int id)
	// Create a (mutable) instance of our definition.  The
	// instance is created to live (temporarily) on some level on
	// the parent movie's display list.
	{
		sprite_instance*	si = new sprite_instance(this, parent->get_root(), parent, id);

		return si;
	}


	// SwiffPlayer.dll 0x10084A30.
	movie_interface*	movie_def_impl::create_instance()
	// Create a playable movie instance from a def.
	{
		movie_root*	m = new movie_root(this);
		assert(m);

		sprite_instance*	root_movie = new sprite_instance(this, m, NULL, -1);
		assert(root_movie);

		root_movie->set_name("_root");
		// 0x10084A85..0x10084AC3 publishes after naming and before retaining
		// the returned wrapper. Its inline assignment matches 0x100738A0.
		set_global_root_movie(root_movie);
		// [CHOSEN, not recovered] S3 bridge: the native wrapper also owns its sprite.
		m->set_root_movie(root_movie);

		m->add_ref();
		return m;
	}


	// SwiffPlayer.dll 0x1007BB50.
	void	sprite_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Create and initialize a sprite, and add it to the movie.
	{
		assert(tag_type == 39);
                
		IF_VERBOSE_PARSE(log_msg("sprite\n"));
		int	character_id = in->read_u16();

		sprite_definition*	ch = new sprite_definition(m, character_id);
		ch->read(in);
		IF_VERBOSE_PARSE(log_msg("sprite: char id = %d\n", character_id));

		m->add_character(character_id, ch);
	}


	//
	// sprite built-in ActionScript methods
	//

	// =============================================================================================
	// the MovieClip methods the shipped sprite builtins object
	// (gameswf_standard_method_map_init, dword_100F8C98) carries and stock 2006 lacks. BF2's menu
	// class framework builds every component with them. Each is reconstructed from the named
	// shipped body:
	//   createEmptyMovieClip(name, depth)  gameswf_sprite_create_empty_movieclip - a fresh sprite
	//                                      definition on the root movie's definition, instanced at
	//                                      `depth` under `name`; two arguments exactly.
	//   attachMovie(symbol, name, depth)   gameswf_sprite_attach_movie - the root definition's
	//                                      exported symbol, instanced at `depth`; three or more args.
	//   getNextHighestDepth()              gameswf_sprite_getNextHighestDepth - a per-sprite counter
	//                                      returned then incremented (+220).
	//   removeMovieClip()                  gameswf_sprite_remove_movieclip - parent->remove(depth).
	//   setMask(mc)                        gameswf_sprite_set_mask - mask stored on the character,
	//                                      mask flagged mask-only (+192/+196); one argument.
	// See GAMESWF_DIVERGENCES.md.
	// =============================================================================================
	static sprite_instance*	sprite_this(const fn_call& fn)
	{
		// [CHOSEN, not recovered] S3 bridge: the 2006 native-call API can
		// omit this_ptr; its environment target supplies the retail receiver.
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
			// [TEMP] RW-89 target trace: a builtin reached without `this` binds to the env target.
			log_error("[sprite_this] this_ptr NULL -> env target '%s'\n", sprite ? sprite->get_name().c_str() : "<none>");
		}
		return sprite;
	}

	// SwiffPlayer.dll 0x1007BFD0; sprite_this supplies the native receiver bridge.
	void	sprite_create_empty_movieclip(const fn_call& fn)
	{
		if (fn.nargs != 2)
		{
			log_error("error: create_empty_movieclip needs two args\n");
			return;
		}
		const tu_string	name = fn.arg(0).to_tu_string();
		const int	depth = bf2x87::toIntTruncate(fn.arg(1).to_number());
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		// [CHOSEN, not recovered] S3 bridge: per-tree root replaces the
		// retail global movie root, through the native typed interface.
		movie_definition_sub* root_def = static_cast<movie_definition_sub*>(sprite->get_root()->get_movie_definition());
		// A one-frame, empty sprite definition owned by the instance (its m_def smart_ptr).
		sprite_definition*	def = new sprite_definition(root_def);
		def->m_frame_count = 1;
		def->m_playlist.resize(1);

		character*	ch = sprite->add_display_object_def(def, name.c_str(), (Uint16) depth);
		// [TEMP] RW-89 target trace. Remove with the tracers.
		log_error("[createEmptyMovieClip] on '%s' -> '%s' depth=%d ch=%p\n", sprite->get_name().c_str(), name.c_str(), depth, ch);
		fn.result->set_as_object_interface(ch);
	}

	// SwiffPlayer.dll 0x10075A40 uses the exported definition's dictionary ID.
	void	sprite_attach_movie(const fn_call& fn)
	{
		if (fn.nargs < 3)
		{
			log_error("error: attach_movie needs atleast three args\n");
			return;
		}
		const tu_string	symbol = fn.arg(0).to_tu_string();
		const tu_string	name = fn.arg(1).to_tu_string();
		const int	depth = bf2x87::toIntTruncate(fn.arg(2).to_number());
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		// [CHOSEN, not recovered] S3 bridge: native per-tree root accessor.
		movie_definition_sub* root_def = static_cast<movie_definition_sub*>(sprite->get_root()->get_movie_definition());
		smart_ptr<resource> res = root_def->get_exported_resource(symbol);
		if (res == NULL)
		{
			*fn.result = as_value();
			return;
		}
		character_def* cdef = res->cast_to_character_def();
		array<swf_event*> handlers;
		character* ch = sprite->add_display_object(cdef->get_character_id(), name.c_str(), handlers,
			static_cast<Uint16>(depth), true, cxform::identity, matrix::identity, 1.0f, 0);
		// [TEMP] RW-89 target trace. Remove with the tracers.
		log_error("[attachMovie] on '%s' symbol='%s' -> '%s' depth=%d ch=%p\n", swf_trace_path(sprite), symbol.c_str(), name.c_str(), depth, ch);
		fn.result->set_as_object_interface(ch);
	}

	// SwiffPlayer.dll 0x1006F9F0; sprite_this is the existing native receiver bridge.
	void	sprite_get_next_highest_depth(const fn_call& fn)
	{
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		fn.result->set_int(sprite->take_next_highest_depth());
	}

	// SwiffPlayer.dll 0x10075950 copies only the definition, transform and ratio,
	// placing in the root at the supplied depth plus 100. It ignores extra arguments.
	void sprite_duplicate_movieclip(const fn_call& fn)
	{
		if (fn.nargs < 2)
		{
			log_error("error: duplicate_movie_clip needs atleast two args\n");
			return;
		}
		const tu_string name = fn.arg(0).to_tu_string();
		const Uint32 depth = static_cast<Uint32>(bf2x87::toIntTruncate(fn.arg(1).to_number())) + 100u;
		// [CHOSEN, not recovered] S3 bridge: typed receiver and per-tree root
		// replace the secondary-interface receiver and retail global root.
		sprite_instance* sprite = sprite_this(fn);
		sprite_instance* root = static_cast<sprite_instance*>(sprite->get_root_movie());
		array<swf_event*> handlers;
		sprite_definition* sprite_def = sprite_definition_of(sprite->m_def.get_ptr());   // root: ID 0
		character* ch = root->add_display_object(sprite_def ? sprite_def->get_character_id() : 0, name.c_str(), handlers,
			static_cast<Uint16>(depth), true, sprite->get_cxform(), sprite->get_matrix(), sprite->get_ratio(), 0);
		fn.result->set_as_object_interface(ch);
	}

	// SwiffPlayer.dll 0x1006F9C0 only requests deferred removal from the parent.
	void	sprite_remove_movieclip(const fn_call& fn)
	{
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		movie*	parent = sprite->get_parent();
		log_error("[removeMovieClip] '%s' depth=%d parent=%p\n",   // [TEMP] RW-88 nav tracer
		          swf_trace_path(sprite), sprite->get_depth(), parent);
		SWF_LIST_TRACE("removeMovieClip '%s' depth=%d", swf_trace_path(sprite), sprite->get_depth());
		if (parent)
		{
			parent->remove_display_object((Uint16) sprite->get_depth());
		}
	}

	// SwiffPlayer.dll 0x10071A30. sprite_this and the per-tree root adapt
	// retail's secondary receiver and global root [CHOSEN, not recovered] S3 bridge.
	void	sprite_start_drag(const fn_call& fn)
	{
		sprite_instance* sprite = sprite_this(fn);
		movie::drag_state st;
		st.m_character = sprite;
		if (fn.nargs > 0) st.m_lock_center = fn.arg(0).to_bool();
		if (!st.m_lock_center)
		{
			int x, y, buttons;
			sprite->get_root_movie()->get_mouse_state(&x, &y, &buttons);
			matrix world = sprite->get_world_matrix();
			point local;
			const float world_x = bf2x87::store(bf2x87::mul(bf2x87::Real(x), bf2x87::Real(20.0f)));
			const float world_y = bf2x87::store(bf2x87::mul(bf2x87::Real(y), bf2x87::Real(20.0f)));
			world.transform_by_inverse(&local, point(world_x, world_y));
			st.m_offset_x = local.m_x;
			st.m_offset_y = local.m_y;
		}
		if (fn.nargs == 5)
		{
			st.m_bound = true;
			st.m_bound_x0 = bf2x87::store(bf2x87::mul(bf2x87::Real(fn.arg(1).to_number()), bf2x87::Real(20.0f)));
			st.m_bound_y0 = bf2x87::store(bf2x87::mul(bf2x87::Real(fn.arg(2).to_number()), bf2x87::Real(20.0f)));
			st.m_bound_x1 = bf2x87::store(bf2x87::mul(bf2x87::Real(fn.arg(3).to_number()), bf2x87::Real(20.0f)));
			st.m_bound_y1 = bf2x87::store(bf2x87::mul(bf2x87::Real(fn.arg(4).to_number()), bf2x87::Real(20.0f)));
		}
		if (sprite != NULL)
		{
			sprite->set_drag_state(st);
		}
	}

	// SwiffPlayer.dll 0x10071B90: a clip stops only its own drag.
	void	sprite_stop_drag(const fn_call& fn)
	{
		// [CHOSEN, not recovered] S3 bridge: native receiver/per-tree root.
		sprite_instance* sprite = sprite_this(fn);
		movie_root* root = sprite->get_root();
		movie::drag_state st;
		root->get_root_movie()->get_drag_state(&st);
		if (st.m_character == sprite) root->stop_drag();
	}

	// SwiffPlayer.dll 0x10071C00. [CHOSEN, not recovered] S3 bridge:
	// the native mask remains owning for issue #7 teardown; retail writes a raw pointer.
	void	sprite_set_mask(const fn_call& fn)
	{
		if (fn.nargs == 0)
		{
			log_error("error: set_mask needs one arg\n");
			return;
		}
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		as_object_interface*	o = fn.arg(0).to_object();
		movie*	mv = o ? o->to_movie() : NULL;
		character*	mask = mv ? mv->cast_to_character() : NULL;
		sprite->set_mask(mask);
	}

	// =============================================================================================
	// the DRAWING API and the IMAGE LOADER of the shipped sprite builtins,
	// reconstructed from the named bodies (coordinates in script pixels, x20 to twips):
	//   lineStyle(thickness, rgb, alpha)   gameswf_sprite_line_style - width = thickness*20, never 0
	//                                      (20 when unset), colour from rgb + alpha*2.55 (255 when
	//                                      absent); appended, referenced 1-based by a NEW path.
	//   moveTo(x, y)                       gameswf_sprite_move_to - starts a new path (inheriting
	//                                      the current styles) when the current one already has
	//                                      edges, else moves its start.
	//   lineTo(x, y)                       gameswf_sprite_line_to - a straight edge from the pen
	//                                      (last anchor, else the path start) with its control point
	//                                      at the midpoint; bound recomputed.
	//   beginFill(rgb, alpha)              gameswf_sprite_begin_fill - endFill first; a solid style
	//                                      appended; a "new shape" marker path then a path carrying
	//                                      fill0 = that style.
	//   endFill()                          gameswf_sprite_end_fill - closes the current path back to
	//                                      its start when the pen is elsewhere; bound recomputed.
	//   beginGradientFill(...)             gameswf_sprite_begin_gradient_fill - five arguments,
	//                                      box matrix, linear/radial bitmap and two new paths.
	//   loadMovie(url)                     gameswf_sprite_loadmovie - removes every child, resolves
	//                                      the image by name (render::create_bitmap_info_file), and
	//                                      places a `_____image` sprite at the next depth holding a
	//                                      `_____image_shape`: one tiled bitmap fill (type 0x40,
	//                                      matrix scale 0.05) over a w*20 x h*20 twips quad. This is
	//                                      the golden run's 320x240 background strip
	//                                      (setTexture id=24 320x240 clamp=0 m=[0.05 0 0|0 0.05 0]).
	// A path's pen position and style inheritance follow the shipped "new path" helper
	// (sub_10076A80): the new path copies fill0/fill1/line and starts at the last anchor.
	// =============================================================================================
	static rgba	rgba_from_script(double rgb, double alpha_pct, bool has_alpha)
	{
		// SwiffPlayer.dll 0x1007C318/0x1007C9B1 uses a dword for RGB;
		// alpha uses a qword conversion and keeps its low byte.
		const int	c = bf2x87::toIntTruncate(rgb);
		const Uint8	a = has_alpha ? static_cast<Uint8>(bf2x87::toInt64Truncate(
			bf2x87::mul(bf2x87::Real(alpha_pct), bf2x87::Real(2.55)))) : 255;
		return rgba((Uint8) ((c >> 16) & 0xFF), (Uint8) ((c >> 8) & 0xFF), (Uint8) (c & 0xFF), a);
	}

	static void	canvas_pen(const path* p, float* x, float* y)
	{
		if (p == NULL)
		{
			*x = 0; *y = 0;
			return;
		}
		if (p->m_edges.size() > 0)
		{
			const edge&	e = p->m_edges[p->m_edges.size() - 1];
			*x = e.m_ax; *y = e.m_ay;
		}
		else
		{
			*x = p->m_ax; *y = p->m_ay;
		}
	}

	// SwiffPlayer.dll 0x10076A80: copy the current path, clear its edges,
	// and start at its final anchor. An empty canvas receives a marker first.
	static path*	canvas_new_path(shape_character_def* def)
	{
		path	np;
		const path*	cur = def->current_path();
		if (cur)
		{
			np = *cur;
			np.m_edges.clear();
			canvas_pen(cur, &np.m_ax, &np.m_ay);
		}
		else
		{
			path marker;
			marker.m_new_shape = true;
			def->append_path(marker);
		}
		def->append_path(np);
		return def->current_path();
	}

	// SwiffPlayer.dll 0x1007C930. With no arguments the shipped body still
	// assigns line slot one, even when no style has been appended.
	void	sprite_line_style(const fn_call& fn)
	{
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		int	idx = 1;
		if (fn.nargs > 0)
		{
			Uint16 width = static_cast<Uint16>(bf2x87::toInt64Truncate(
				bf2x87::mul(bf2x87::Real(fn.arg(0).to_number()), bf2x87::Real(20.0))));
			if (width == 0) width = 20;
			rgba	color(0, 0, 0, 255);
			if (fn.nargs > 1)
			{
				const double rgb = fn.arg(1).to_number();
				const double alpha = fn.nargs > 2 ? fn.arg(2).to_number() : 100.0;
				color = rgba_from_script(rgb, alpha, fn.nargs > 2);
			}
			idx = def->add_line_style(color, width) + 1;
		}
		canvas_new_path(def)->m_line = idx;
	}

	// SwiffPlayer.dll 0x1007CB60 creates/resolves the canvas even on bad arity.
	void	sprite_move_to(const fn_call& fn)
	{
		sprite_instance* sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def* def = sprite->get_canvas_def();
		if (fn.nargs != 2)
		{
			log_error("error: move_to needs two args\n");
			return;
		}
		const float	x = (float) (fn.arg(0).to_number() * 20.0);
		const float	y = (float) (fn.arg(1).to_number() * 20.0);
		path*	cur = def->current_path();
		if (cur != NULL && cur->m_edges.size() == 0)
		{
			cur->m_ay = y;
			cur->m_ax = x;
			return;
		}
		// 0x1007CBED stores only Y into the new path's X start. Preserve
		// this shipped defect; the inherited Y start is left alone.
		canvas_new_path(def)->m_ax = y;
	}

	// SwiffPlayer.dll 0x1007CA20 stores the X delta as float before computing Y.
	void	sprite_line_to(const fn_call& fn)
	{
		sprite_instance* sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def* def = sprite->get_canvas_def();
		if (fn.nargs != 2)
		{
			log_error("error: line_to needs two args\n");
			return;
		}
		path*	cur = def->current_path();
		float	px, py;
		canvas_pen(cur, &px, &py);
		const float dx = bf2x87::store(bf2x87::sub(
			bf2x87::mul(bf2x87::Real(fn.arg(0).to_number()), bf2x87::Real(20.0)), bf2x87::Real(px)));
		const bf2x87::Real dy = bf2x87::sub(
			bf2x87::mul(bf2x87::Real(fn.arg(1).to_number()), bf2x87::Real(20.0)), bf2x87::Real(py));
		cur->m_edges.push_back(edge(
			bf2x87::store(bf2x87::add(bf2x87::mul(bf2x87::Real(dx), bf2x87::Real(0.5f)), bf2x87::Real(px))),
			bf2x87::store(bf2x87::add(bf2x87::mul(dy, bf2x87::Real(0.5f)), bf2x87::Real(py))),
			bf2x87::store(bf2x87::add(bf2x87::Real(dx), bf2x87::Real(px))),
			bf2x87::store(bf2x87::add(bf2x87::Real(py), dy))));
		rect bound;
		def->compute_bound(&bound);
		def->set_bound(bound);
	}

	// SwiffPlayer.dll 0x1007C1D0.
	void	sprite_end_fill(const fn_call& fn)
	{
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		path*	cur = def->current_path();
		if (cur && cur->m_edges.size() > 0)
		{
			float	px, py;
			canvas_pen(cur, &px, &py);
			const bf2x87::Real dx = bf2x87::sub(bf2x87::Real(cur->m_ax), bf2x87::Real(px));
			const bf2x87::Real dy = bf2x87::sub(bf2x87::Real(cur->m_ay), bf2x87::Real(py));
			if (dx != bf2x87::Real(0) || dy != bf2x87::Real(0))
			{
				cur->m_edges.push_back(edge(
					bf2x87::store(bf2x87::add(bf2x87::mul(dx, bf2x87::Real(0.5f)), bf2x87::Real(px))),
					bf2x87::store(bf2x87::add(bf2x87::mul(dy, bf2x87::Real(0.5f)), bf2x87::Real(py))),
					bf2x87::store(bf2x87::add(dx, bf2x87::Real(px))),
					bf2x87::store(bf2x87::add(dy, bf2x87::Real(py)))));
			}
		}
		rect bound;
		def->compute_bound(&bound);
		def->set_bound(bound);
	}

	// SwiffPlayer.dll 0x1007C2C0 appends a marker and a fresh fill-only path.
	void	sprite_begin_fill(const fn_call& fn)
	{
		sprite_end_fill(fn);
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		int	idx = 0;
		if (fn.nargs > 0)
		{
			const double rgb = fn.arg(0).to_number();
			const double alpha = fn.nargs > 1 ? fn.arg(1).to_number() : 100.0;
			idx = def->add_fill_style(rgba_from_script(rgb, alpha, fn.nargs > 1)) + 1;
		}
		// the shipped pair: a "new shape" marker path, then the path that carries the fill
		path	marker;
		marker.m_new_shape = true;
		def->append_path(marker);
		path	p;
		p.m_fill0 = idx;
		def->append_path(p);
	}

	// SwiffPlayer.dll 0x1007C3B0.
	void	sprite_begin_gradient_fill(const fn_call& fn)
	{
		if (fn.nargs != 5)
		{
			log_error("error: line_to needs five args\n");
			return;
		}
		as_object_interface* box = fn.arg(4).get_type() == as_value::OBJECT
			? fn.arg(4).m_object_value : NULL;
		as_value value;
		matrix input;
		if (!box->get_member("matrixType", &value)
			|| std::strcmp(value.to_string(), "box") != 0) return;
		box->get_member("x", &value);
		const float x = (float)value.to_number();
		box->get_member("y", &value);
		const float y = (float)value.to_number();
		box->get_member("w", &value);
		const float width = (float)value.to_number();
		box->get_member("h", &value);
		const float height = (float)value.to_number();
		box->get_member("r", &value);
		const float rotation = (float)value.to_number();
		input.m_[0][2] = (float)(((long double)width * 0.5f + x) * 20.0f);
		input.m_[1][2] = (float)(((long double)height * 0.5f + y) * 20.0f);
		input.m_[0][0] = (float)(1.0L / (32768.0f / ((long double)width * 20.0f)));
		input.m_[1][1] = (float)(1.0L / (32768.0f / ((long double)height * 20.0f)));

		fill_style style;
		matrix gradient;
		const tu_string kind(fn.arg(0).to_string());
		int type = style.get_type();
		if (kind == "linear")
		{
			gradient.set_identity();
			gradient.m_[0][2] = 128.0f;
			gradient.m_[1][2] = 0.0f;
			gradient.set_scale_rotation(1.0f / 128.0f, 1.0f / 128.0f, -rotation);
			type = 0x10;
		}
		else if (kind == "radial")
		{
			gradient.set_identity();
			gradient.concatenate_translation(32.0f, 32.0f);
			gradient.concatenate_scale(1.0f / 512.0f);
			type = 0x12;
		}
		matrix inverse;
		inverse.set_inverse(input);
		gradient.concatenate(inverse);
		style.set_gradient(type, gradient);

		// [CHOSEN, not recovered] S3 bridge to typed native Array storage.
		const auto object_arg = [&fn](int index) -> as_object_interface*
		{
			const as_value& argument = fn.arg(index);
			return argument.get_type() == as_value::OBJECT ? argument.m_object_value : NULL;
		};
		const array<as_value>& colors = array_object_values(object_arg(1));
		const array<as_value>& alphas = array_object_values(object_arg(2));
		const array<as_value>& ratios = array_object_values(object_arg(3));
		for (int i = 0; i < colors.size(); ++i)
		{
			const int rgb = bf2x87::toIntTruncate(colors[i].to_number());
			const float alpha = (float)alphas[i].to_number();
			const Uint8 ratio = (Uint8)bf2x87::toInt64Truncate(ratios[i].to_number());
			// The PE multiplies by 2.56f and retains the low byte, without clamping.
			const Uint8 opacity = (Uint8)bf2x87::toInt64Truncate((long double)alpha * 2.56f);
			gradient_record record;
			record.m_ratio = ratio;
			record.m_color = rgba((Uint8)(rgb >> 16), (Uint8)(rgb >> 8), (Uint8)rgb, opacity);
			style.append_gradient(record);
		}
		sprite_instance* sprite = sprite_this(fn);
		// [CHOSEN, not recovered] S3 bridge: the native bitmap owner is per-tree.
		style.generate_gradient_bitmap(sprite->get_root()->m_def.get_ptr());
		shape_character_def* def = sprite->get_canvas_def();
		const int fill = def->append_fill_style(style) + 1;
		path p;
		p.m_new_shape = true;
		def->append_path(p);
		p.m_new_shape = false;
		p.m_fill0 = fill;
		p.m_fill1 = 0;
		p.m_line = 0;
		def->append_path(p);
	}

	// SwiffPlayer.dll 0x1007BBF0. Runtime-definition ownership and the native
	// receiver/root accessors retain the S3 bridges described below.
	void	sprite_load_movie(const fn_call& fn)
	{
		sprite_instance* sprite = sprite_this(fn);
		if (sprite == NULL) return;
		sprite->remove_all_display_objects();
		if (fn.nargs < 1)
		{
			log_error("error: sprite_load_movie needs atleast one arg\n");
			return;
		}
		// [TEMP] RW-89 target trace: which clip loads which image, where it sits. Remove with the tracers.
		{
			const matrix	wm = sprite->get_world_matrix();
			character*	par = sprite->get_parent() ? sprite->get_parent()->cast_to_character() : NULL;
			log_error("[loadMovie] target='%s' url='%s' world=[%g %g %g | %g %g %g] depth=%d parent='%s'\n",
				  sprite->get_name().c_str(), fn.arg(0).to_string(),
				  wm.m_[0][0], wm.m_[0][1], wm.m_[0][2], wm.m_[1][0], wm.m_[1][1], wm.m_[1][2],
				  sprite->get_depth(), par ? par->get_name().c_str() : "<none>");
		}

		const tu_string	url = fn.arg(0).to_tu_string();
		smart_ptr<bitmap_info>	bi = render::create_bitmap_info_file(url.c_str());
		if (bi == NULL)
		{
			return;
		}
		const int	w = bi->m_original_width;
		const int	h = bi->m_original_height;
		smart_ptr<bitmap_character> bc = new bitmap_character(bi.get_ptr());
		bc->set_character_id(0);

		movie*	root = sprite->get_root_movie();
		movie_definition_sub*	root_def = root
			? static_cast<movie_definition_sub*>(root->get_movie_definition()) : NULL;
		if (root_def == NULL)
		{
			return;
		}

		// The `_____image` sprite at the next depth.
		sprite_definition*	idef = new sprite_definition(root_def);
		idef->m_frame_count = 1;
		idef->m_playlist.resize(1);
		character*	img = sprite->add_display_object_def(idef, "_____image", (Uint16) sprite->take_next_highest_depth());
		sprite_instance*	img_sprite = img ? static_cast<sprite_instance*>(img) : NULL;
		if (img_sprite == NULL)
		{
			return;
		}

		// Its `_____image_shape`: one tiled bitmap fill over the texture-sized quad.
		smart_ptr<shape_character_def>	sh = new shape_character_def;
		sh->set_character_id(0);
		fill_style	fs;
		matrix	bm;
		bm.set_identity();
		bm.m_[0][0] = 0.05f;
		bm.m_[1][1] = 0.05f;
		fs.set_bitmap(bc.get_ptr(), bm, false);
		const int	idx = sh->append_fill_style(fs) + 1;

		path	marker;
		marker.m_new_shape = true;
		sh->append_path(marker);
		path	p(0.0f, 0.0f, idx, 0, 0);
		const float	w20 = (float) w * 20.0f;
		const float	h20 = (float) h * 20.0f;
		p.m_edges.push_back(edge(0.0f, 0.0f, w20, 0.0f));
		p.m_edges.push_back(edge(w20, h20, w20, h20));
		p.m_edges.push_back(edge(0.0f, h20, 0.0f, h20));
		p.m_edges.push_back(edge(0.0f, 0.0f, 0.0f, 0.0f));
		sh->append_path(p);
		rect bound;
		sh->compute_bound(&bound);
		sh->set_bound(bound);

		// [CHOSEN, not recovered] S3 bridge: retain the image sprite's canvas
		// alias while the native image/canvas integration awaits its runtime gates.
		// The placed generic character also retains its definition (0x1007F130).
		// [RETIRE-WHEN: swf-runtime-image-owner] Retire when native character/definition
		// ownership follows the recovered lifetime and repeated image/remove/reload gates pass.
		img_sprite->m_canvas = sh.get_ptr();
		img_sprite->add_display_object_def(sh.get_ptr(), "_____image_shape", 0);
	}

	// SwiffPlayer.dll 0x1006F980.
	// [CHOSEN, not recovered] S3 bridge: borrow the native VM target without this_ptr.
	void	sprite_play(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);
		sprite->set_play_state(movie_interface::PLAY);
	}

	// SwiffPlayer.dll 0x1006F9A0.
	// [CHOSEN, not recovered] S3 bridge: borrow the native VM target without this_ptr.
	void	sprite_stop(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);
		sprite->set_play_state(movie_interface::STOP);
	}

	// SwiffPlayer.dll 0x10075780: the numeric arm subtracts one before a
	// truncating qword conversion and passes its low dword (0x10075808).
	void	sprite_goto_and_play(const fn_call& fn)
	{
		// [CHOSEN, not recovered] S2 bridge: the 2006 VM can omit this_ptr;
		// borrow its target. Retail's native-call ABI supplies the receiver directly.
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);

		if (fn.nargs < 1)
		{
			log_error("error: sprite_goto_and_play needs one arg\n");
			return;
		}

		// SwiffPlayer.dll 0x10075827 calls the queued-label slot, not +0x1C.
		if (fn.arg(0).get_type() == as_value::STRING)
		{
			sprite->queue_goto_labeled_frame(fn.arg(0).to_string());
		}
		else if (fn.arg(0).get_type() == as_value::NUMBER)
		{
			const int target_frame = bf2x87::lowWord(bf2x87::toInt64Truncate(
				bf2x87::sub(bf2x87::Real(fn.arg(0).to_number()), bf2x87::Real(1))));
			sprite->goto_frame(target_frame);
		}
		sprite->set_play_state(movie_interface::PLAY);
	}

	// SwiffPlayer.dll 0x10075860; qword conversion at 0x100758E5.
	void	sprite_goto_and_stop(const fn_call& fn)
	{
		// [CHOSEN, not recovered] S2 bridge: see sprite_goto_and_play's receiver.
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);

		if (fn.nargs < 1)
		{
			log_error("error: sprite_goto_and_stop needs one arg\n");
			return;
		}

		// SwiffPlayer.dll 0x10075904 calls queued-label slot +0xAC. The queue
		// drains in advance, after the mouse pass has finished walking the children.
		if (fn.arg(0).get_type() == as_value::STRING)
		{
			sprite->queue_goto_labeled_frame(fn.arg(0).to_string());
		}
		else if (fn.arg(0).get_type() == as_value::NUMBER)
		{
			const int target_frame = bf2x87::lowWord(bf2x87::toInt64Truncate(
				bf2x87::sub(bf2x87::Real(fn.arg(0).to_number()), bf2x87::Real(1))));
			sprite->goto_frame(target_frame);
		}
		sprite->set_play_state(movie_interface::STOP);
	}

	// SwiffPlayer.dll 0x100719F0 applies nextFrame immediately, then stops.
	// [CHOSEN, not recovered] S3 bridge: borrow the native VM target without this_ptr.
	void	sprite_next_frame(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);

		int frame_count = sprite->get_frame_count();
		int current_frame = sprite->get_current_frame();
		if (current_frame < frame_count)
		{
			sprite->apply_goto_frame(current_frame + 1);
		}
		sprite->set_play_state(movie_interface::STOP);
	}

	// SwiffPlayer.dll 0x1007E2D0: lazy registration, in the shipped order.
	static void	sprite_builtins_init()
	{
		if (s_sprite_builtins)
		{
			return;
		}

		s_sprite_builtins = new as_object;
		s_sprite_builtins->set_member("play", &sprite_play);
		s_sprite_builtins->set_member("stop", &sprite_stop);
		s_sprite_builtins->set_member("gotoAndStop", &sprite_goto_and_stop);
		s_sprite_builtins->set_member("gotoAndPlay", &sprite_goto_and_play);
		s_sprite_builtins->set_member("nextFrame", &sprite_next_frame);
		s_sprite_builtins->set_member("loadMovie", &sprite_load_movie);
		s_sprite_builtins->set_member("startDrag", &sprite_start_drag);
		s_sprite_builtins->set_member("stopDrag", &sprite_stop_drag);
		s_sprite_builtins->set_member("duplicateMovieClip", &sprite_duplicate_movieclip);
		s_sprite_builtins->set_member("removeMovieClip", &sprite_remove_movieclip);
		s_sprite_builtins->set_member("attachMovie", &sprite_attach_movie);
		s_sprite_builtins->set_member("setMask", &sprite_set_mask);
		s_sprite_builtins->set_member("getNextHighestDepth", &sprite_get_next_highest_depth);
		// [CHOSEN, not recovered] S3 bridge: retain the native timer owner.
		// The timer subsystem owns recovery of 0x1007E030 / 0x1007E210.
		s_sprite_builtins->set_member("setInterval", &timer_setinterval);
		s_sprite_builtins->set_member("clearInterval", &timer_clearinterval);
		s_sprite_builtins->set_member("createEmptyMovieClip", &sprite_create_empty_movieclip);
		s_sprite_builtins->set_member("lineTo", &sprite_line_to);
		s_sprite_builtins->set_member("moveTo", &sprite_move_to);
		s_sprite_builtins->set_member("beginFill", &sprite_begin_fill);
		s_sprite_builtins->set_member("beginGradientFill", &sprite_begin_gradient_fill);
		s_sprite_builtins->set_member("endFill", &sprite_end_fill);
		s_sprite_builtins->set_member("lineStyle", &sprite_line_style);

	}

	// SwiffPlayer.dll 0x1006FA40 deletes the shared object directly.
	static void	sprite_builtins_clear()
	{
		if (s_sprite_builtins)
		{
			delete s_sprite_builtins;
			s_sprite_builtins = 0;
		}
	}





	//
	// end_tag
	//

	// end_tag doesn't actually need to exist.

	// SwiffPlayer.dll 0x1006FA70 is empty in the release image.
	void	end_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 0);
		assert(in->get_position() == in->get_tag_end_position());
	}


	//
	// remove_object_2
	//

	
	struct remove_object_2 : public execute_tag
	{
		// SwiffPlayer.dll 0x100719B0: implicit destruction has no owned members.
		int	m_depth;

		// SwiffPlayer.dll 0x1006F860.
		remove_object_2() : m_depth(-1) {}

		// SwiffPlayer.dll 0x1006F870.
		void	read(stream* in)
		{
			m_depth = in->read_u16();
		}

		// SwiffPlayer.dll 0x1006F890.
		virtual void	execute(movie* m)
		{
			m->remove_display_object(m_depth);
			IF_VERBOSE_DEBUG(log_msg(">>>>>>>>> character removed at %d\n", m_depth));
		}

		// SwiffPlayer.dll 0x1006F8D0.
		virtual void	execute_state(movie* m)
		{
			execute(m);
		}

		virtual void	execute_state_reverse(movie* m, int frame)
		{
			// reverse of remove is to re-add the previous object.
			execute_tag*	last_add = m->find_previous_replace_or_add_tag(frame, m_depth, -1);
			if (last_add)
			{
				last_add->execute_state(m);
			}
			else
			{
				log_error("reverse REMOVE can't find previous replace or add tag(%d, %d)\n",
					  frame, m_depth);
					
			}
		}

		// SwiffPlayer.dll 0x1006F8F0.
		virtual bool	is_remove_tag() const { return true; }

		//  the goto pass does nothing with a remove (0x1006F8E0 is a
		// nullsub); the state builder reads its depth.
		// SwiffPlayer.dll 0x1006F8E0.
		virtual void	execute_goto(movie* /*m*/) {}
		virtual int	get_tag_depth() const { return m_depth; }
	};


	// SwiffPlayer.dll 0x1006F900.
	void	remove_object_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 28);

		remove_object_2*	t = new remove_object_2;
		t->read(in);

		IF_VERBOSE_PARSE(log_msg("remove_object_2(%d)\n", t->m_depth));

		m->add_execute_tag(t);
	}


	// SwiffPlayer.dll 0x1006FA80.
	void	button_sound_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 17);

		int	button_character_id = in->read_u16();
		button_character_definition* ch = (button_character_definition*) m->get_character_def(button_character_id);
		assert(ch != NULL);

		ch->read(in, tag_type, m);
	}


	// SwiffPlayer.dll 0x1006FAB0.
	void	button_character_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 7 || tag_type == 34);

		int	character_id = in->read_u16();

		button_character_definition*	ch = new button_character_definition;
		// [CHOSEN, not recovered] S3 bridge: the S2 native constructor has no ID argument.
		ch->set_character_id(character_id);
		ch->read(in, tag_type, m);

		m->add_character(character_id, ch);
	}


	//
	// export
	//


	// SwiffPlayer.dll 0x10074030: font, character, sound, then bitmap lookup.
	void	export_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load an export tag (for exposing internal resources of m)
	{
		assert(tag_type == 56);

		int	count = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("export: count = %d\n", count));

		// Read the exports.
		for (int i = 0; i < count; i++)
		{
			Uint16	id = in->read_u16();
			char*	symbol_name = in->read_string();
			IF_VERBOSE_PARSE(log_msg("export: id = %d, name = %s\n", id, symbol_name));

			if (font* f = m->get_font(id))
			{
				// Expose this font for export.
				m->export_resource(id, tu_string(symbol_name), f);
			}
			else if (character_def* ch = m->get_character_def(id))
			{
				// Expose this movie/button/whatever for export.
				m->export_resource(id, tu_string(symbol_name), ch);
			}
			else if (sound_sample* ch = m->get_sound_sample(id))
			{
				m->export_resource(id, tu_string(symbol_name), ch);
			}
			// export BITMAP characters too.
			//
			// Bitmap characters live in a SEPARATE map (m_bitmap_characters, filled by
			// add_bitmap_character) and are NOT in m_characters, so the get_character_def branch
			// above never sees them - stock gameswf drops every image-named export on the floor and
			// does not even warn, it falls into the "don't know how to export" arm below.
			//
			// That silently breaks BF2's entire menu-image pipeline: each menu bitmap is an undecoded
			// DefineBitsLossless character whose ExportAssets symbol IS its external file path
			// ("images/UIcomponents/button-bkg-7.png"), which the host resolves through
			// ISwiffRender::loadTexture. Measured on mainMenu.swf: 12 image-named exports, all 12
			// DefineBits characters, 0 reachable before this branch existed.
			// See GAMESWF_DIVERGENCES.md.
			else if (bitmap_character_def* bch = m->get_bitmap_character(id))
			{
				// SwiffPlayer.dll 0x1007417E: the descriptor owns the name.
				static_cast<named_bitmap_info*>(bch->get_bitmap_info())->m_name = symbol_name;
				m->export_resource(id, tu_string(symbol_name), bch);
			}
			else
			{
				log_error("export error: don't know how to export resource '%s'\n",
					  symbol_name);
			}

			delete [] symbol_name;
		}
	}


	//
	// import
	//


	// SwiffPlayer.dll 0x1007D830; font/character import dispatch and the
	// no-recurse recording path retain their shipped ordering and ownership.
	void	import_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load an import tag (for pulling in external resources)
	{
		assert(tag_type == 57);

		char*	source_url = in->read_string();
		int	count = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("import: source_url = %s, count = %d\n", source_url, count));

		// Try to load the source movie into the movie library.
		movie_definition_sub*	source_movie = NULL;

		if (s_no_recurse_while_loading == false)
		{
			source_movie = create_library_movie_sub(source_url);
			if (source_movie == NULL)
			{
				// Give up on imports.
				log_error("can't import movie from url %s\n", source_url);
				return;
			}
		}

		// Get the imports.
		for (int i = 0; i < count; i++)
		{
			Uint16	id = in->read_u16();
			char*	symbol_name = in->read_string();
			IF_VERBOSE_PARSE(log_msg("import: id = %d, name = %s\n", id, symbol_name));

			if (s_no_recurse_while_loading)
			{
				m->add_import(source_url, id, symbol_name);
			}
			else
			{
				// @@ TODO get rid of this, always use
				// s_no_recurse_while_loading, change
				// create_movie_sub().

				smart_ptr<resource> res = source_movie->get_exported_resource(symbol_name);
				if (res == NULL)
				{
					log_error("import error: resource '%s' is not exported from movie '%s'\n",
						  symbol_name, source_url);
				}
				else if (font* f = res->cast_to_font())
				{
					// Add this shared font to the currently-loading movie.
					m->add_font(id, f);
				}
				else if (character_def* ch = res->cast_to_character_def())
				{
					// Add this character to the loading movie.
					m->add_character(id, ch);
				}
				else
				{
					log_error("import error: resource '%s' from movie '%s' has unknown type\n",
						  symbol_name, source_url);
				}
			}

			delete [] symbol_name;
		}

		delete [] source_url;
	}
}


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
