// gameswf_impl.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Some implementation for SWF player.

// Useful links:
//
// http://sswf.sourceforge.net/SWFalexref.html
// http://www.openswf.org

// @@ Need to break this file into pieces


#include "base/tu_file.h"
#include "base/utility.h"
#include "gameswf_action.h"
#include "gameswf_button.h"
#include "gameswf_impl.h"
#include "gameswf_font.h"
#include "gameswf_fontlib.h"
#include "gameswf_log.h"
#include "gameswf_morph2.h"
#include "gameswf_render.h"
#include "gameswf_shape.h"
#include "gameswf_stream.h"
#include "gameswf_styles.h"
#include "gameswf_dlist.h"
#include "gameswf_timers.h"
#include "base/image.h"
#include "base/jpeg.h"
#include "base/zlib_adapter.h"
#include <string.h>	// for memset
#include <typeinfo>
#include <float.h>


#if TU_CONFIG_LINK_TO_ZLIB
#include <zlib.h>
#endif // TU_CONFIG_LINK_TO_ZLIB


namespace gameswf
{
	bool	s_verbose_action = false;
	bool	s_verbose_parse = false;

#ifndef NDEBUG
	bool	s_verbose_debug = true;
#else
	bool	s_verbose_debug = false;
#endif


	void	set_verbose_action(bool verbose)
	// Enable/disable log messages re: actions.
	{
		s_verbose_action = verbose;
	}


	void	set_verbose_parse(bool verbose)
	// Enable/disable log messages re: parsing the movie.
	{
		s_verbose_parse = verbose;
	}


	static bool	s_use_cache_files = true;

	void	set_use_cache_files(bool use_cache)
	// Enable/disable attempts to read cache files when loading
	// movies.
	{
		s_use_cache_files = use_cache;
	}


	// Keep a table of loader functions for the different tag types.
	static hash<int, loader_function>	s_tag_loaders;

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
		s_opener_function = opener;
	}

	//
	// progress callback stuff (from Vitaly)
	//

	static progress_callback	s_progress_function = NULL;

	void	register_progress_callback(progress_callback progress_handle)
	// Host calls this to register a function for progress bar handling
	// during loading movies.
	{
		s_progress_function = progress_handle;
	}
	
	//
	// some utility stuff
	//


	// [DICE-MOD SwiffPlayer #48] execute_actions (index walk over the live list) is gone: its
	// only caller, sprite_instance::do_actions, now drains the list the way the shipped body does.


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


	character*	character_def::create_character_instance(movie* parent, int id)
	// Default.  Make a generic_character.
	{
		return new generic_character(this, parent, id);
	}


	//
	// ref_counted
	//


	ref_counted::ref_counted()
		:
		m_ref_count(0),
		m_weak_proxy(0)
	{
	}

	ref_counted::~ref_counted()
	{
		assert(m_ref_count == 0);

		if (m_weak_proxy)
		{
			m_weak_proxy->notify_object_died();
			m_weak_proxy->drop_ref();
		}
	}

	void	ref_counted::add_ref() const
	{
		assert(m_ref_count >= 0);
		m_ref_count++;
	}

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
		drag_state	st;
		get_drag_state(&st);
		if (this == st.m_character)
		{
			// We're being dragged!
			int	x, y, buttons;
			get_root_movie()->get_mouse_state(&x, &y, &buttons);

			point	world_mouse(PIXELS_TO_TWIPS(x), PIXELS_TO_TWIPS(y));
			if (st.m_bound)
			{
				// Clamp mouse coords within a defined rect.
				world_mouse.m_x =
					fclamp(world_mouse.m_x, st.m_bound_x0, st.m_bound_x1);
				world_mouse.m_y =
					fclamp(world_mouse.m_y, st.m_bound_y0, st.m_bound_y1);
			}

			if (st.m_lock_center)
			{
				matrix	world_mat = get_world_matrix();
				point	local_mouse;
				world_mat.transform_by_inverse(&local_mouse, world_mouse);

				matrix	parent_world_mat;
				if (m_parent)
				{
					parent_world_mat = m_parent->get_world_matrix();
				}

				point	parent_mouse;
				parent_world_mat.transform_by_inverse(&parent_mouse, world_mouse);
					
				// Place our origin so that it coincides with the mouse coords
				// in our parent frame.
				matrix	local = get_matrix();
				local.m_[0][2] = parent_mouse.m_x;
				local.m_[1][2] = parent_mouse.m_y;
				set_matrix(local);
			}
			else
			{
				// Implement relative drag...
			}
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
		array<array<execute_tag*> >	   m_init_action_list;	// Init actions for each frame.
		stringi_hash<int>	           m_named_frames;	// 0-based frame #'s
		stringi_hash<smart_ptr<resource> > m_exports;

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
		uint32	m_file_length;

		jpeg::input*	m_jpeg_in;


		movie_def_impl(create_bitmaps_flag cbf, create_font_shapes_flag cfs)
			:
			m_create_bitmaps(cbf),
			m_create_font_shapes(cfs),
			m_frame_rate(30.0f),
			m_frame_count(0),
			m_version(0),
			m_loading_frame(0),
			m_jpeg_in(0)
		{
		}

		~movie_def_impl()
		{
			// Release our playlist data.
			{for (int i = 0, n = m_playlist.size(); i < n; i++)
			{
				for (int j = 0, m = m_playlist[i].size(); j < m; j++)
				{
					delete m_playlist[i][j];
				}
			}}

			// Release init action data.
			{for (int i = 0, n = m_init_action_list.size(); i < n; i++)
			{
				for (int j = 0, m = m_init_action_list[i].size(); j < m; j++)
				{
					delete m_init_action_list[i][j];
				}
			}}

			assert(m_jpeg_in == NULL);	// It's supposed to be cleaned up in read()
		}


		movie_interface*	create_instance();

		// ...
		int	get_frame_count() const { return m_frame_count; }
		float	get_frame_rate() const { return m_frame_rate; }
		float	get_width_pixels() const { return ceilf(TWIPS_TO_PIXELS(m_frame_size.width())); }
		float	get_height_pixels() const { return ceilf(TWIPS_TO_PIXELS(m_frame_size.height())); }

		virtual int	get_version() const { return m_version; }

		virtual int	get_loading_frame() const { return m_loading_frame; }

		uint32	get_file_bytes() const { return m_file_length; }

		/* movie_def_impl */
		virtual create_bitmaps_flag	get_create_bitmaps() const
		// Returns DO_CREATE_BITMAPS if we're supposed to
		// initialize our bitmap infos, or DO_NOT_INIT_BITMAPS
		// if we're supposed to create blank placeholder
		// bitmaps (to be init'd later explicitly by the host
		// program).
		{
			return m_create_bitmaps;
		}

		/* movie_def_impl */
		virtual create_font_shapes_flag	get_create_font_shapes() const
		// Returns DO_LOAD_FONT_SHAPES if we're supposed to
		// initialize our font shape info, or
		// DO_NOT_LOAD_FONT_SHAPES if we're supposed to not
		// create any (vector) font glyph shapes, and instead
		// rely on precached textured fonts glyphs.
		{
			return m_create_font_shapes;
		}

		virtual void	add_bitmap_info(bitmap_info* bi)
		// All bitmap_info's used by this movie should be
		// registered with this API.
		{
			m_bitmap_list.push_back(bi);
		}


		// [DICE-MOD SwiffPlayer: walk m_exports, keeping the entries whose resource is a bitmap
		// character. The NAME is the ExportAssets symbol, which for BF2's menu bitmaps is the
		// external image path. See gameswf.h and GAMESWF_DIVERGENCES.md.]
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

		// [DICE-MOD SwiffPlayer #37] see gameswf_impl.h.
		virtual bool	get_exported_font_name(const font* f, tu_string* out)
		{
			if (f == NULL || f->get_name() == NULL) return false;
			for (stringi_hash<smart_ptr<resource> >::const_iterator it = m_exports.begin();
			     it != m_exports.end(); ++it)
			{
				resource*	res = it.get_value().get_ptr();
				if (res == NULL) continue;
				font*	ef = res->cast_to_font();
				if (ef == NULL || ef->get_name() == NULL) continue;
				if (strcmp(ef->get_name(), f->get_name()) == 0)
				{
					*out = it.get_key();
					return true;
				}
			}
			return false;
		}

		virtual int	get_bitmap_info_count() const { return m_bitmap_list.size(); }
		virtual bitmap_info*	get_bitmap_info(int i) const
		{
			return m_bitmap_list[i].get_ptr();
		}

		virtual void	export_resource(const tu_string& symbol, resource* res)
		// Expose one of our resources under the given symbol,
		// for export.  Other movies can import it.
		{
			// SWF sometimes exports the same thing more than once!
			m_exports.set(symbol, res);
		}

		virtual smart_ptr<resource>	get_exported_resource(const tu_string& symbol)
		// Get the named exported resource, if we expose it.
		// Otherwise return NULL.
		{
			smart_ptr<resource>	res;
			m_exports.get(symbol, &res);
			return res;
		}

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

		void	add_character(int character_id, character_def* c)
		{
			assert(c);
			m_characters.add(character_id, c);
		}

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

		bool	get_labeled_frame(const char* label, int* frame_number)
		// Returns 0-based frame #
		{
			return m_named_frames.get(label, frame_number);
		}

		void	add_font(int font_id, font* f)
		{
			assert(f);
			m_fonts.add(font_id, f);
		}

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

		bitmap_character_def*	get_bitmap_character(int character_id)
		{
			smart_ptr<bitmap_character_def>	ch;
			m_bitmap_characters.get(character_id, &ch);
			assert(ch == NULL || ch->get_ref_count() > 1);
			return ch.get_ptr();
		}

		void	add_bitmap_character(int character_id, bitmap_character_def* ch)
		{
			assert(ch);
			m_bitmap_characters.add(character_id, ch);

			add_bitmap_info(ch->get_bitmap_info());
		}

		sound_sample*	get_sound_sample(int character_id)
		{
			smart_ptr<sound_sample>	ch;
			m_sound_samples.get(character_id, &ch);
			assert(ch == NULL || ch->get_ref_count() > 1);
			return ch.get_ptr();
		}

		virtual void	add_sound_sample(int character_id, sound_sample* sam)
		{
			assert(sam);
			m_sound_samples.add(character_id, sam);
		}

		void	add_execute_tag(execute_tag* e)
		{
			assert(e);
			m_playlist[m_loading_frame].push_back(e);
		}

		void	add_init_action(int sprite_id, execute_tag* e)
		// Need to execute the given tag before entering the
		// currently-loading frame for the first time.
		//
		// @@ AFAIK, the sprite_id is totally pointless -- correct?
		{
			assert(e);
			m_init_action_list[m_loading_frame].push_back(e);
		}

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

		void	set_jpeg_loader(jpeg::input* j_in)
		// Set an input object for later loading DefineBits
		// images (JPEG images without the table info).
		{
			assert(m_jpeg_in == NULL);
			m_jpeg_in = j_in;
		}

		jpeg::input*	get_jpeg_loader()
		// Get the jpeg input loader, to load a DefineBits
		// image (one without table info).
		{
			return m_jpeg_in;
		}


		virtual const array<execute_tag*>&	get_playlist(int frame_number) { return m_playlist[frame_number]; }

		/* movie_def_impl */
		virtual const array<execute_tag*>*	get_init_actions(int frame_number)
		{
			return &m_init_action_list[frame_number];
		}

		/* movie_def_impl */
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

			tu_file*	original_in = NULL;
			if (compressed)
			{
#if TU_CONFIG_LINK_TO_ZLIB == 0
				log_error("movie_def_impl::read(): unable to read zipped SWF data; TU_CONFIG_LINK_TO_ZLIB is 0\n");
				return;
#endif

				IF_VERBOSE_PARSE(log_msg("file is compressed.\n"));
				original_in = in;

				// Uncompress the input as we read it.
				in = zlib_adapter::make_inflater(original_in);

				// Subtract the size of the 8-byte header, since
				// it's not included in the compressed
				// stream length.
				file_end_pos = m_file_length - 8;
			}

			stream	str(in);

			m_frame_size.read(&str);
			m_frame_rate = str.read_u16() / 256.0f;
			m_frame_count = str.read_u16();

			m_playlist.resize(m_frame_count);
			m_init_action_list.resize(m_frame_count);

			IF_VERBOSE_PARSE(m_frame_size.print());
			IF_VERBOSE_PARSE(log_msg("frame rate = %f, frames = %d\n", m_frame_rate, m_frame_count));

			while ((Uint32) str.get_position() < file_end_pos)
			{
				int	tag_type = str.open_tag();

				if (s_progress_function != NULL)
				{
					s_progress_function((Uint32) str.get_position(), file_end_pos);
				}

				loader_function	lf = NULL;
				//IF_VERBOSE_PARSE(log_msg("tag_type = %d\n", tag_type));
				if (tag_type == 1)
				{
					// show frame tag -- advance to the next frame.
					IF_VERBOSE_PARSE(log_msg("  show_frame\n"));
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

			if (m_jpeg_in)
			{
				delete m_jpeg_in;
				m_jpeg_in = NULL;
			}

			if (original_in)
			{
				// Done with the zlib_adapter.
				delete in;
			}
		}


		/* movie_def_impl */
		void	get_owned_fonts(array<font*>* fonts)
		// Fill up *fonts with fonts that we own.
		{
			assert(fonts);
			fonts->resize(0);

			array<int>	font_ids;

			for (hash<int, smart_ptr<font> >::iterator it = m_fonts.begin();
			     it != m_fonts.end();
			     ++it)
			{
				font*	f = it->second.get_ptr();
				if (f->get_owning_movie() == this)
				{
					// Sort by character id, so the ordering is
					// consistent for cache read/write.
					int	id = it->first;

					// Insert in correct place.
					int	insert;
					for (insert = 0; insert < font_ids.size(); insert++)
					{
						if (font_ids[insert] > id)
						{
							// We want to insert here.
							break;
						}
					}
					fonts->insert(insert, f);
					font_ids.insert(insert, id);
				}
			}
		}


		/* movie_def_impl */
		void	generate_font_bitmaps()
		// Generate bitmaps for our fonts, if necessary.
		{
			// Collect list of fonts.
			array<font*>	fonts;
			get_owned_fonts(&fonts);
			fontlib::generate_font_bitmaps(fonts, this);
		}


		// Increment this when the cache data format changes.
		#define CACHE_FILE_VERSION 4


		/* movie_def_impl */
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


		/* movie_def_impl */
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
		bool			m_on_event_load_called;

		// Flags for event handlers
		bool			m_on_event_xmlsocket_ondata_called;
		bool			m_on_event_xmlsocket_onxml_called;
		bool			m_on_event_load_progress_called;
		array<Timer *>	m_interval_timers;

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
			m_on_event_load_called(false),
			m_on_event_xmlsocket_ondata_called(false),
			m_on_event_xmlsocket_onxml_called(false),
			m_on_event_load_progress_called(false)
		{
			assert(m_def != NULL);

			set_display_viewport(0, 0, (int) m_def->get_width_pixels(), (int) m_def->get_height_pixels());
		}

		~movie_root()
		{
			assert(m_def != NULL);
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

		void	set_display_viewport(int x0, int y0, int w, int h)
		{
			m_viewport_x0 = x0;
			m_viewport_y0 = y0;
			m_viewport_width = w;
			m_viewport_height = h;

			// Recompute pixel scale.
			float	scale_x = m_viewport_width / TWIPS_TO_PIXELS(m_def->m_frame_size.width());
			float	scale_y = m_viewport_height / TWIPS_TO_PIXELS(m_def->m_frame_size.height());
			m_pixel_scale = fmax(scale_x, scale_y);
		}


		void	notify_mouse_state(int x, int y, int buttons)
		// The host app uses this to tell the movie where the
		// user's mouse pointer is.
		{
			m_mouse_x = x;
			m_mouse_y = y;
			m_mouse_buttons = buttons;
		}

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

		// [DICE-MOD SwiffPlayer #51] the shipped root's +0xB4 / +0xB8: the character holding the
		// pointer since its press. A weak reference, as the stock active entity was.
		movie*	get_active_entity() { smart_ptr<movie> p(m_mouse_button_state.m_active_entity); return p.get_ptr(); }
		void	set_active_entity(movie* m) { m_mouse_button_state.m_active_entity = m; }

		movie*	get_root_movie() { return m_movie.get_ptr(); }


		void	stop_drag()
		{
			m_drag_state.m_character = NULL;
		}


		movie_definition*	get_movie_definition() { return m_movie->get_movie_definition(); }

		uint32	get_file_bytes() const
		{
			return m_def->get_file_bytes();
		}

		virtual int    add_interval_timer(void *timer)
		{
			Timer *ptr = static_cast<Timer *>(timer);
			
			m_interval_timers.push_back(ptr);
			return m_interval_timers.size();
		}
	
		virtual void    clear_interval_timer(int x)
		{
			m_interval_timers.remove(x-1);
			//m_interval_timers[x]->clearInterval();
		}
	
		virtual void    do_something(void *timer)
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
		}
		
		// 0-based!!
		int	get_current_frame() const { return m_movie->get_current_frame(); }
		float	get_frame_rate() const { return m_def->get_frame_rate(); }

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

		void	set_background_color(const rgba& color)
		{
			m_background_color = color;
		}

		// [DICE-MOD SwiffPlayer: the shipped getter (sub_100736C0) - see gameswf.h.]
		rgba	get_background_color() const
		{
			return m_background_color;
		}

		void	set_background_alpha(float alpha)
		{
			m_background_color.m_a = iclamp(frnd(alpha * 255.0f), 0, 255);
		}

		float	get_background_alpha() const
		{
			return m_background_color.m_a / 255.0f;
		}

		float	get_timer() const { return m_timer; }

		void	restart() { m_movie->restart(); }
		
		void	advance(float delta_time)
		{
			int i;
			if (m_on_event_load_called == false)
			{
				// Must do loading events.  For child sprites this is
				// done by the dlist, but root movies don't get added
				// to a dlist, so we do it here.
				m_on_event_load_called = true;
				m_movie->on_event_load();
			}
#if 0
			// Must check the socket connection for data
			if (m_on_event_xmlsocket_ondata_called == true) {
				m_movie->on_event_xmlsocket_ondata();
			}
			
			if (m_on_event_xmlsocket_onxml_called == true) {
				m_movie->on_event_xmlsocket_onxml();
			}


			// Must check the progress of the MovieClip being loaded
			if (m_on_event_load_progress_called == true) {
				m_movie->on_event_load_progress();				
			}
#endif
			if (m_interval_timers.size() > 0) {
				for (i=0; i<m_interval_timers.size(); i++) {
					if (m_interval_timers[i]->expired()) {
						// printf("FIXME: Interval Timer Expired!\n");
						//m_movie->on_event_interval_timer();
						m_movie->do_something(m_interval_timers[i]);
						// clear_interval_timer(m_interval_timers[i]->getIntervalID()); // FIXME: we shouldn't really disable the timer here
					}
				}
			}
			
			
			m_timer += delta_time;
			// @@ TODO handle multi-frame catch-up stuff
			// here, and make it optional.  Make
			// movie::advance() a fixed framerate w/ no
			// dt.

			// Handle the mouse. [DICE-MOD SwiffPlayer #51] one recursive pass down the tree, every
			// character its own state machine (sprite 0x10080010, text 0x10088650, button 0x100A3020).
			// Stock 2006 picked a single "topmost entity" here and fed generate_mouse_button_events.
			m_movie->mouse_update(false);

			m_movie->advance(delta_time);
		}

		// 0-based!!
		void	goto_frame(int target_frame_number) { m_movie->goto_frame(target_frame_number); }

		virtual bool	has_looped() const { return m_movie->has_looped(); }

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

			m_movie->display();

			gameswf::render::end_display();
		}

		virtual bool	goto_labeled_frame(const char* label)
		{
			// [DICE-MOD SwiffPlayer #45] a label applies at once on the root sprite too (the
			// numeric goto_frame above is the deferred form).
			return m_movie->goto_labeled_frame(label);
		}

		virtual void	set_play_state(play_state s) { m_movie->set_play_state(s); }
		virtual play_state	get_play_state() const { return m_movie->get_play_state(); }

		/* movie_root */
		virtual void	set_variable(const char* path_to_var, const char* new_value)
		{
			m_movie->set_variable(path_to_var, new_value);
		}

		/* movie_root */
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value)
		{
			m_movie->set_variable(path_to_var, new_value);
		}

		virtual const char*	get_variable(const char* path_to_var) const
		{
			return m_movie->get_variable(path_to_var);
		}

		/*movie_root*/
		// For ActionScript interfacing convenience.
		virtual const char*	call_method(const char* method_name, const char* method_arg_fmt, ...)
		{
			assert(m_movie != NULL);

			va_list	args;
			va_start(args, method_arg_fmt);
			const char*	result = m_movie->call_method_args(method_name, method_arg_fmt, args);
			va_end(args);

			return result;
		}

		/*movie_root*/
		virtual const char*	call_method_args(const char* method_name, const char* method_arg_fmt, va_list args)
		{
			assert(m_movie != NULL);
			return m_movie->call_method_args(method_name, method_arg_fmt, args);
		}

		virtual void	set_visible(bool visible) { m_movie->set_visible(visible); }
		virtual bool	get_visible() const { return m_movie->get_visible(); }

		virtual void * get_userdata() { return m_userdata; }
		virtual void set_userdata(void * ud ) { m_userdata = ud;  }

		virtual void	attach_display_callback(const char* path_to_object, void (*callback)(void* user_ptr), void* user_ptr)
		{
			m_movie->attach_display_callback(path_to_object, callback, user_ptr);
		}
	};


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
			register_tag_loader(5, remove_object_2_loader);
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
		}
	}



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

		tu_file*	original_in = NULL;
		if (compressed)
		{
#if TU_CONFIG_LINK_TO_ZLIB == 0
			log_error("get_movie_info(): can't read zipped SWF data; TU_CONFIG_LINK_TO_ZLIB is 0!\n");
			return;
#endif
			original_in = in;

			// Uncompress the input as we read it.
			in = zlib_adapter::make_inflater(original_in);

			// Subtract the size of the 8-byte header, since
			// it's not included in the compressed
			// stream length.
			file_length -= 8;
		}

		stream	str(in);

		rect	frame_size;
		frame_size.read(&str);

		float	local_frame_rate = str.read_u16() / 256.0f;
		int	local_frame_count = str.read_u16();

		if (version) *version = local_version;
		if (width) *width = int(frame_size.width() / 20.0f + 0.5f);
		if (height) *height = int(frame_size.height() / 20.0f + 0.5f);
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
		delete original_in;
	}



	// [DICE-MOD SwiffPlayer: see gameswf.h. Default preserves stock behaviour.]
	static create_bitmaps_flag	s_default_create_bitmaps = DO_LOAD_BITMAPS;

	void	set_default_create_bitmaps(create_bitmaps_flag cbf)
	{
		s_default_create_bitmaps = cbf;
	}

	movie_definition*	create_movie(const char* filename)
	// Create the movie definition from the specified .swf file.
	{
		return create_movie_sub(filename);
	}


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

		// [DICE-MOD SwiffPlayer: was hardcoded DO_LOAD_BITMAPS - see set_default_create_bitmaps.]
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

				m->generate_font_bitmaps();	// can't read cache, so generate font texture data.
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

	movie_interface* get_current_root()
	{
		assert(s_current_root != NULL);
		return s_current_root;
	}

	void set_current_root(movie_interface* m)
	{
		assert(m != NULL);
		s_current_root = m;
	}

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


	movie_definition*	create_library_movie(const char* filename)
	// Try to load a movie from the given url, if we haven't
	// loaded it already.  Add it to our library on success, and
	// return a pointer to it.
	{
		return create_library_movie_sub(filename);
	}


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

			save_stuff()
			{
				// Save.
				m_original_rh = get_render_handler();
				m_original_sh = get_sound_handler();
				set_render_handler(NULL);
				set_sound_handler(NULL);
			}

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

			if (m->get_current_frame() == movie_def->get_frame_count() - 1)
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


	void	null_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Silently ignore the contents of this tag.
	{
	}

	void	frame_label_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Label the current frame of m with the name from the stream.
	{
		char*	n = in->read_string();
		m->add_frame_name(n);
		delete [] n;
	}

	struct set_background_color : public execute_tag
	{
		rgba	m_color;

		void	execute(movie* m)
		{
			float	current_alpha = m->get_background_alpha();
			m_color.m_a = frnd(current_alpha * 255.0f);
			m->set_background_color(m_color);
		}

		void	execute_state(movie* m)
		{
			execute(m);
		}

		void	read(stream* in)
		{
			m_color.read_rgb(in);

			IF_VERBOSE_PARSE(log_msg("  set_background_color: (%d %d %d)\n",
						 m_color.m_r, m_color.m_g, m_color.m_b));
		}
	};


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

	void	jpeg_tables_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load JPEG compression tables that can be used to load
	// images further along in the stream.
	{
		assert(tag_type == 8);

#if TU_CONFIG_LINK_TO_JPEGLIB
		jpeg::input*	j_in = jpeg::input::create_swf_jpeg2_header_only(in->get_underlying_stream());
		assert(j_in);

		m->set_jpeg_loader(j_in);
#endif // TU_CONFIG_LINK_TO_JPEGLIB
	}


	void	define_bits_jpeg_loader(stream* in, int tag_type, movie_definition_sub* m)
	// A JPEG image without included tables; those should be in an
	// existing jpeg::input object stored in the movie.
	{
		assert(tag_type == 6);

		Uint16	character_id = in->read_u16();

		//
		// Read the image data.
		//
		bitmap_info*	bi = NULL;

		if (m->get_create_bitmaps() == DO_LOAD_BITMAPS)
		{
#if TU_CONFIG_LINK_TO_JPEGLIB
			jpeg::input*	j_in = m->get_jpeg_loader();
			assert(j_in);
			j_in->discard_partial_buffer();

			image::rgb*	im = image::read_swf_jpeg2_with_tables(j_in);
			bi = render::create_bitmap_info_rgb(im);
			delete im;
#else
			log_error("gameswf is not linked to jpeglib -- can't load jpeg image data!\n");
			bi = render::create_bitmap_info_empty();
#endif
		}
		else
		{
			bi = render::create_bitmap_info_empty();
		}

		assert(bi->get_ref_count() == 0);

		bitmap_character*	ch = new bitmap_character(bi);

		m->add_bitmap_character(character_id, ch);
	}


	void	define_bits_jpeg2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 21);
		
		Uint16	character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  define_bits_jpeg2_loader: charid = %d pos = 0x%x\n", character_id, in->get_position()));

		//
		// Read the image data.
		//
		
		bitmap_info*	bi = NULL;

		if (m->get_create_bitmaps() == DO_LOAD_BITMAPS)
		{
#if TU_CONFIG_LINK_TO_JPEGLIB
			image::rgb* im = image::read_swf_jpeg2(in->get_underlying_stream());
			bi = render::create_bitmap_info_rgb(im);
			delete im;
#else
			log_error("gameswf is not linked to jpeglib -- can't load jpeg image data!\n");
			bi = render::create_bitmap_info_empty();
#endif
		}
		else
		{
			bi = render::create_bitmap_info_empty();
		}

		assert(bi->get_ref_count() == 0);

		bitmap_character*	ch = new bitmap_character(bi);

		m->add_bitmap_character(character_id, ch);
	}


#if TU_CONFIG_LINK_TO_ZLIB
	void	inflate_wrapper(tu_file* in, void* buffer, int buffer_bytes)
	// Wrapper function -- uses Zlib to uncompress in_bytes worth
	// of data from the input file into buffer_bytes worth of data
	// into *buffer.
	{
		assert(in);
		assert(buffer);
		assert(buffer_bytes > 0);

		int err;
		z_stream d_stream; /* decompression stream */

		d_stream.zalloc = (alloc_func)0;
		d_stream.zfree = (free_func)0;
		d_stream.opaque = (voidpf)0;

		d_stream.next_in  = 0;
		d_stream.avail_in = 0;

		d_stream.next_out = (Byte*) buffer;
		d_stream.avail_out = (uInt) buffer_bytes;

		err = inflateInit(&d_stream);
		if (err != Z_OK) {
			log_error("error: inflate_wrapper() inflateInit() returned %d\n", err);
			return;
		}

		Uint8	buf[1];

		for (;;) {
			// Fill a one-byte (!) buffer.
			buf[0] = in->read_byte();
			d_stream.next_in = &buf[0];
			d_stream.avail_in = 1;

			err = inflate(&d_stream, Z_SYNC_FLUSH);
			if (err == Z_STREAM_END) break;
			if (err != Z_OK)
			{
				log_error("error: inflate_wrapper() inflate() returned %d\n", err);
			}
		}

		err = inflateEnd(&d_stream);
		if (err != Z_OK)
		{
			log_error("error: inflate_wrapper() inflateEnd() return %d\n", err);
		}
	}
#endif // TU_CONFIG_LINK_TO_ZLIB


	void	define_bits_jpeg3_loader(stream* in, int tag_type, movie_definition_sub* m)
	// loads a define_bits_jpeg3 tag. This is a jpeg file with an alpha
	// channel using zlib compression.
	{
		assert(tag_type == 35);

		Uint16	character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  define_bits_jpeg3_loader: charid = %d pos = 0x%x\n", character_id, in->get_position()));

		Uint32	jpeg_size = in->read_u32();
		Uint32	alpha_position = in->get_position() + jpeg_size;

		bitmap_info*	bi = NULL;

		if (m->get_create_bitmaps() == DO_LOAD_BITMAPS)
		{
#if TU_CONFIG_LINK_TO_JPEGLIB == 0 || TU_CONFIG_LINK_TO_ZLIB == 0
			log_error("gameswf is not linked to jpeglib/zlib -- can't load jpeg/zipped image data!\n");
			bi = render::create_bitmap_info_empty();
#else
			//
			// Read the image data.
			//
		
			// Read rgb data.
			image::rgba*	im = image::read_swf_jpeg3(in->get_underlying_stream());

			// Read alpha channel.
			in->set_position(alpha_position);

			int	buffer_bytes = im->m_width * im->m_height;
			Uint8*	buffer = new Uint8[buffer_bytes];

			inflate_wrapper(in->get_underlying_stream(), buffer, buffer_bytes);

			for (int i = 0; i < buffer_bytes; i++)
			{
				im->m_data[4*i+3] = buffer[i];
			}

			delete [] buffer;

			bi = render::create_bitmap_info_rgba(im);

			delete im;
#endif

		}
		else
		{
			bi = render::create_bitmap_info_empty();
		}

		// Create bitmap character.
		bitmap_character*	ch = new bitmap_character(bi);

		m->add_bitmap_character(character_id, ch);
	}


	void	define_bits_lossless_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 20 || tag_type == 36);

		Uint16	character_id = in->read_u16();
		Uint8	bitmap_format = in->read_u8();	// 3 == 8 bit, 4 == 16 bit, 5 == 32 bit
		Uint16	width = in->read_u16();
		Uint16	height = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  defbitslossless2: tag_type = %d, id = %d, fmt = %d, w = %d, h = %d\n",
				tag_type,
				character_id,
				bitmap_format,
				width,
				height));

		bitmap_info*	bi = NULL;
		if (m->get_create_bitmaps() == DO_LOAD_BITMAPS)
		{
#if TU_CONFIG_LINK_TO_ZLIB == 0
			log_error("gameswf is not linked to zlib -- can't load zipped image data!\n");
			return;
#else
			if (tag_type == 20)
			{
				// RGB image data.
				image::rgb*	image = image::create_rgb(width, height);

				if (bitmap_format == 3)
				{
					// 8-bit data, preceded by a palette.

					const int	bytes_per_pixel = 1;
					int	color_table_size = in->read_u8();
					color_table_size += 1;	// !! SWF stores one less than the actual size

					int	pitch = (width * bytes_per_pixel + 3) & ~3;

					int	buffer_bytes = color_table_size * 3 + pitch * height;
					Uint8*	buffer = new Uint8[buffer_bytes];

					inflate_wrapper(in->get_underlying_stream(), buffer, buffer_bytes);
					assert(in->get_position() <= in->get_tag_end_position());

					Uint8*	color_table = buffer;

					for (int j = 0; j < height; j++)
					{
						Uint8*	image_in_row = buffer + color_table_size * 3 + j * pitch;
						Uint8*	image_out_row = image::scanline(image, j);
						for (int i = 0; i < width; i++)
						{
							Uint8	pixel = image_in_row[i * bytes_per_pixel];
							image_out_row[i * 3 + 0] = color_table[pixel * 3 + 0];
							image_out_row[i * 3 + 1] = color_table[pixel * 3 + 1];
							image_out_row[i * 3 + 2] = color_table[pixel * 3 + 2];
						}
					}

					delete [] buffer;
				}
				else if (bitmap_format == 4)
				{
					// 16 bits / pixel
					const int	bytes_per_pixel = 2;
					int	pitch = (width * bytes_per_pixel + 3) & ~3;

					int	buffer_bytes = pitch * height;
					Uint8*	buffer = new Uint8[buffer_bytes];

					inflate_wrapper(in->get_underlying_stream(), buffer, buffer_bytes);
					assert(in->get_position() <= in->get_tag_end_position());
			
					for (int j = 0; j < height; j++)
					{
						Uint8*	image_in_row = buffer + j * pitch;
						Uint8*	image_out_row = image::scanline(image, j);
						for (int i = 0; i < width; i++)
						{
							Uint16	pixel = image_in_row[i * 2] | (image_in_row[i * 2 + 1] << 8);
					
							// @@ How is the data packed???  I'm just guessing here that it's 565!
							image_out_row[i * 3 + 0] = (pixel >> 8) & 0xF8;	// red
							image_out_row[i * 3 + 1] = (pixel >> 3) & 0xFC;	// green
							image_out_row[i * 3 + 2] = (pixel << 3) & 0xF8;	// blue
						}
					}
			
					delete [] buffer;
				}
				else if (bitmap_format == 5)
				{
					// 32 bits / pixel, input is ARGB format (???)
					const int	bytes_per_pixel = 4;
					int	pitch = width * bytes_per_pixel;

					int	buffer_bytes = pitch * height;
					Uint8*	buffer = new Uint8[buffer_bytes];

					inflate_wrapper(in->get_underlying_stream(), buffer, buffer_bytes);
					assert(in->get_position() <= in->get_tag_end_position());
			
					// Need to re-arrange ARGB into RGB.
					for (int j = 0; j < height; j++)
					{
						Uint8*	image_in_row = buffer + j * pitch;
						Uint8*	image_out_row = image::scanline(image, j);
						for (int i = 0; i < width; i++)
						{
							Uint8	a = image_in_row[i * 4 + 0];
							Uint8	r = image_in_row[i * 4 + 1];
							Uint8	g = image_in_row[i * 4 + 2];
							Uint8	b = image_in_row[i * 4 + 3];
							image_out_row[i * 3 + 0] = r;
							image_out_row[i * 3 + 1] = g;
							image_out_row[i * 3 + 2] = b;
							a = a;	// Inhibit warning.
						}
					}

					delete [] buffer;
				}

//				bitmap_character*	ch = new bitmap_character(image);
				bi = render::create_bitmap_info_rgb(image);
				delete image;

// 				// add image to movie, under character id.
// 				m->add_bitmap_character(character_id, ch);
			}
			else
			{
				// RGBA image data.
				assert(tag_type == 36);

				image::rgba*	image = image::create_rgba(width, height);

				if (bitmap_format == 3)
				{
					// 8-bit data, preceded by a palette.

					const int	bytes_per_pixel = 1;
					int	color_table_size = in->read_u8();
					color_table_size += 1;	// !! SWF stores one less than the actual size

					int	pitch = (width * bytes_per_pixel + 3) & ~3;

					int	buffer_bytes = color_table_size * 4 + pitch * height;
					Uint8*	buffer = new Uint8[buffer_bytes];

					inflate_wrapper(in->get_underlying_stream(), buffer, buffer_bytes);
					assert(in->get_position() <= in->get_tag_end_position());

					Uint8*	color_table = buffer;

					for (int j = 0; j < height; j++)
					{
						Uint8*	image_in_row = buffer + color_table_size * 4 + j * pitch;
						Uint8*	image_out_row = image::scanline(image, j);
						for (int i = 0; i < width; i++)
						{
							Uint8	pixel = image_in_row[i * bytes_per_pixel];
							image_out_row[i * 4 + 0] = color_table[pixel * 4 + 0];
							image_out_row[i * 4 + 1] = color_table[pixel * 4 + 1];
							image_out_row[i * 4 + 2] = color_table[pixel * 4 + 2];
							image_out_row[i * 4 + 3] = color_table[pixel * 4 + 3];
						}
					}

					delete [] buffer;
				}
				else if (bitmap_format == 4)
				{
					// 16 bits / pixel
					const int	bytes_per_pixel = 2;
					int	pitch = (width * bytes_per_pixel + 3) & ~3;

					int	buffer_bytes = pitch * height;
					Uint8*	buffer = new Uint8[buffer_bytes];

					inflate_wrapper(in->get_underlying_stream(), buffer, buffer_bytes);
					assert(in->get_position() <= in->get_tag_end_position());
			
					for (int j = 0; j < height; j++)
					{
						Uint8*	image_in_row = buffer + j * pitch;
						Uint8*	image_out_row = image::scanline(image, j);
						for (int i = 0; i < width; i++)
						{
							Uint16	pixel = image_in_row[i * 2] | (image_in_row[i * 2 + 1] << 8);
					
							// @@ How is the data packed???  I'm just guessing here that it's 565!
							image_out_row[i * 4 + 0] = 255;			// alpha
							image_out_row[i * 4 + 1] = (pixel >> 8) & 0xF8;	// red
							image_out_row[i * 4 + 2] = (pixel >> 3) & 0xFC;	// green
							image_out_row[i * 4 + 3] = (pixel << 3) & 0xF8;	// blue
						}
					}
			
					delete [] buffer;
				}
				else if (bitmap_format == 5)
				{
					// 32 bits / pixel, input is ARGB format

					inflate_wrapper(in->get_underlying_stream(), image->m_data, width * height * 4);
					assert(in->get_position() <= in->get_tag_end_position());
			
					// Need to re-arrange ARGB into RGBA.
					for (int j = 0; j < height; j++)
					{
						Uint8*	image_row = image::scanline(image, j);
						for (int i = 0; i < width; i++)
						{
							Uint8	a = image_row[i * 4 + 0];
							Uint8	r = image_row[i * 4 + 1];
							Uint8	g = image_row[i * 4 + 2];
							Uint8	b = image_row[i * 4 + 3];
							image_row[i * 4 + 0] = r;
							image_row[i * 4 + 1] = g;
							image_row[i * 4 + 2] = b;
							image_row[i * 4 + 3] = a;
						}
					}
				}

				bi = render::create_bitmap_info_rgba(image);
//				bitmap_character*	ch = new bitmap_character(image);
				delete image;

//	 			// add image to movie, under character id.
//	 			m->add_bitmap_character(character_id, ch);
			}
#endif // TU_CONFIG_LINK_TO_ZLIB
		}
		else
		{
			bi = render::create_bitmap_info_empty();
		}
		assert(bi->get_ref_count() == 0);

		bitmap_character*	ch = new bitmap_character(bi);

		// add image to movie, under character id.
		m->add_bitmap_character(character_id, ch);
	}


	void	define_shape_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 2
		       || tag_type == 22
		       || tag_type == 32);

		Uint16	character_id = in->read_u16();
		IF_VERBOSE_PARSE(log_msg("  shape_loader: id = %d\n", character_id));

		shape_character_def*	ch = new shape_character_def;
		ch->read(in, tag_type, true, m);

		IF_VERBOSE_PARSE(log_msg("  bound rect:"); ch->get_bound().print());

		m->add_character(character_id, ch);
	}

	void define_shape_morph_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 46);
		Uint16 character_id = in->read_u16();
		IF_VERBOSE_PARSE(log_msg("  shape_morph_loader: id = %d\n", character_id));
		morph2_character_def* morph = new morph2_character_def;
		morph->read(in, tag_type, true, m);
		m->add_character(character_id, morph);
	}

	//
	// font loaders
	//


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

		void	attach_to(character* ch) const
		{
			ch->set_event_handler(m_event, m_method);
		}

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

			// 14 bits reserved, 18 bits used

			static const event_id	s_code_bits[19] =
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
				event_id::CONSTRUCT,	// bit 18 [DICE-MOD SwiffPlayer #27]
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

				IF_VERBOSE_PARSE(
					log_msg("  char_id = %d\n"
						"  depth = %d\n"
						"  mat = \n",
						m_character_id,
						m_depth);
					m_matrix.print());

				if (in->get_position() < in->get_tag_end_position())
				{
					m_color_transform.read_rgb(in);
					IF_VERBOSE_PARSE(log_msg("  cxform:\n"); m_color_transform.print());
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
				IF_VERBOSE_PARSE(log_msg("  depth = %d\n", m_depth));

				if (has_char) {
					m_character_id = in->read_u16();
					IF_VERBOSE_PARSE(log_msg("  char id = %d\n", m_character_id));
				}

				if (has_matrix) {
					m_has_matrix = true;
					m_matrix.read(in);
					IF_VERBOSE_PARSE(log_msg("  mat:\n"); m_matrix.print());
				}
				if (has_cxform) {
					m_has_cxform = true;
					m_color_transform.read_rgba(in);
					IF_VERBOSE_PARSE(log_msg("  cxform:\n"); m_color_transform.print());
				}
				
				if (has_ratio) {
					m_ratio = (float)in->read_u16() / (float)65535;
					IF_VERBOSE_PARSE(log_msg("  ratio: %f\n", m_ratio));
				}
				
				if (has_name) {
					m_name = in->read_string();
					IF_VERBOSE_PARSE(log_msg("  name = %s\n", m_name ? m_name : "<null>"));
				}
				if (has_clip_bracket) {
					m_clip_depth = in->read_u16(); 
					IF_VERBOSE_PARSE(log_msg("  clip_depth = %d\n", m_clip_depth));
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

					IF_VERBOSE_PARSE(log_msg("  actions: flags = 0x%X\n", all_flags));

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
                                
				//log_msg("place object at depth %i\n", m_depth);
			}
		}

		
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
				break;
			}
		}

		void	execute_state(movie* m)
		{
			execute(m);
		}

		// [DICE-MOD SwiffPlayer #45] see execute_tag::execute_goto: the goto pass does nothing
		// with a place (0x10075760 is a nullsub); the state builder reads its depth and kind.
		void	execute_goto(movie* /*m*/) {}
		bool	is_place_tag() const { return true; }
		int	get_tag_depth() const { return m_depth; }
		int	get_place_type() const { return (int) m_place_type; }

		void	execute_state_reverse(movie* m, int frame)
		{
			switch (m_place_type)
			{
			case PLACE:
				// reverse of add is remove
				m->remove_display_object(m_depth, m_tag_type == 4 ? m_character_id : -1);
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


	
	void	place_object_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 4 || tag_type == 26);

		IF_VERBOSE_PARSE(log_msg("  place_object_2\n"));

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


	struct sprite_definition : public movie_definition_sub
	{
		movie_definition_sub*	     m_movie_def;		// parent movie.
		array<array<execute_tag*> >  m_playlist;	// movie control events for each frame.
		stringi_hash<int>	     m_named_frames;	// stores 0-based frame #'s
		int	                     m_frame_count;
		int	                     m_loading_frame;
		sprite_definition(movie_definition_sub* m)
			:
			m_movie_def(m),
			m_frame_count(0),
			m_loading_frame(0)
		{
			assert(m_movie_def);
		}

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
		virtual float	get_width_pixels() const { return 1; }
		virtual float	get_height_pixels() const { return 1; }
		virtual int	get_frame_count() const { return m_frame_count; }
		virtual float	get_frame_rate() const { return m_movie_def->get_frame_rate(); }
		virtual int	get_loading_frame() const { return m_loading_frame; }
		virtual int	get_version() const { return m_movie_def->get_version(); }
		virtual void	add_character(int id, character_def* ch) { log_error("add_character tag appears in sprite tags!\n"); }
		virtual void	add_font(int id, font* ch) { log_error("add_font tag appears in sprite tags!\n"); }
		virtual font*	get_font(int id) { return m_movie_def->get_font(id); }
		virtual void	set_jpeg_loader(jpeg::input* j_in) { assert(0); }
		virtual jpeg::input*	get_jpeg_loader() { return NULL; }
		virtual bitmap_character_def*	get_bitmap_character(int id) { return m_movie_def->get_bitmap_character(id); }
		virtual void	add_bitmap_character(int id, bitmap_character_def* ch) { log_error("add_bc appears in sprite tags!\n"); }
		virtual sound_sample*	get_sound_sample(int id) { return m_movie_def->get_sound_sample(id); }
		virtual void	add_sound_sample(int id, sound_sample* sam) { log_error("add sam appears in sprite tags!\n"); }

		// @@ would be nicer to not inherit these...
		virtual create_bitmaps_flag	get_create_bitmaps() const { assert(0); return DO_LOAD_BITMAPS; }
		virtual create_font_shapes_flag	get_create_font_shapes() const { assert(0); return DO_LOAD_FONT_SHAPES; }
		virtual int	get_bitmap_info_count() const { assert(0); return 0; }
		virtual bitmap_info*	get_bitmap_info(int i) const { assert(0); return NULL; }
		virtual void	add_bitmap_info(bitmap_info* bi) { assert(0); }

		virtual void	export_resource(const tu_string& symbol, resource* res) { log_error("can't export from sprite\n"); }
		virtual smart_ptr<resource>	get_exported_resource(const tu_string& sym) { return m_movie_def->get_exported_resource(sym); }
		virtual bool	get_exported_font_name(const font* f, tu_string* out) { return m_movie_def->get_exported_font_name(f, out); }	// [DICE-MOD #37]
		virtual void	add_import(const char* source_url, int id, const char* symbol) { assert(0); }
		virtual void	visit_imported_movies(import_visitor* v) { assert(0); }
		virtual void	resolve_import(const char* source_url, movie_definition* d) { assert(0); }
		virtual character_def*	get_character_def(int id)
		{
			return m_movie_def->get_character_def(id);
		}
		virtual void	generate_font_bitmaps() { assert(0); }


		virtual void	output_cached_data(tu_file* out, const cache_options& options)
		{
			// Nothing to do.
			return;
		}
		virtual void	input_cached_data(tu_file* in)
		{
			// Nothing to do.
			return;
		}

		virtual movie_interface*	create_instance()
		{
			return NULL;
		}

		// overloads from character_def
		virtual character*	create_character_instance(movie* parent, int id);


		/* sprite_definition */
		virtual void	add_execute_tag(execute_tag* c)
		{
			m_playlist[m_loading_frame].push_back(c);
		}

		/* sprite_definition */
		virtual void	add_init_action(int sprite_id, execute_tag* c)
		{
			// Sprite def's should not have do_init_action tags in them!  (@@ correct?)
			log_error("sprite_definition::add_init_action called!  Ignored.\n");
		}

		/* sprite_definition */
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
		bool	get_labeled_frame(const char* label, int* frame_number)
		// Returns 0-based frame #
		{
			return m_named_frames.get(label, frame_number);
		}

		const array<execute_tag*>&	get_playlist(int frame_number)
		// frame_number is 0-based
		{
			return m_playlist[frame_number];
		}

		/* sprite_definition */
		virtual const array<execute_tag*>*	get_init_actions(int frame_number)
		{
			// Sprites do not have init actions in their
			// playlists!  Only the root movie
			// (movie_def_impl) does (@@ correct?)
			return NULL;
		}


		/* sprite_definition */
		void	read(stream* in)
		// Read the sprite info.  Consists of a series of tags.
		{
			int	tag_end = in->get_tag_end_position();

			m_frame_count = in->read_u16();
			// ALEX: some SWF files have been seen that have 0-frame sprites.
			// The Macromedia player behaves as if they have 1 frame.
			if (m_frame_count < 1)
			{
				m_frame_count = 1;
			}
			m_playlist.resize(m_frame_count);	// need a playlist for each frame

			IF_VERBOSE_PARSE(log_msg("  frames = %d\n", m_frame_count));

			m_loading_frame = 0;

			while ((Uint32) in->get_position() < (Uint32) tag_end)
			{
				int	tag_type = in->open_tag();
				loader_function lf = NULL;
				if (tag_type == 1)
				{
					// show frame tag -- advance to the next frame.
					IF_VERBOSE_PARSE(log_msg("  show_frame (sprite)\n"));
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

			IF_VERBOSE_PARSE(log_msg("  -- sprite END --\n"));
		}
	};


	//
	// sprite_instance
	//


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

	struct sprite_instance : public character
	{
		smart_ptr<movie_definition_sub>	m_def;
		movie_root*	m_root;

		display_list	m_display_list;
		array<action_buffer*>	m_action_list;

		play_state	m_play_state;
		int		m_current_frame;
		float		m_time_remainder;
		bool		m_update_frame;
		bool		m_has_looped;
		bool	        m_accept_anim_moves;	// once we've been moved by ActionScript, don't accept moves from anim tags.
		array<bool>	m_init_actions_executed;	// a bit-array class would be ideal for this

		as_environment	m_as_environment;

		enum mouse_state
		{
			UP = 0,
			DOWN,
			OVER
		};
		mouse_state m_mouse_state;

		// [DICE-MOD SwiffPlayer: Object.registerClass support - the class prototype this instance
		// chains to (its __proto__) and whether the class constructor has run. See
		// bind_registered_class() and gameswf.h character_def::m_registered_class.]
		smart_ptr<as_object_interface>	m_class_proto;
		bool	m_class_bound;
		bool	m_class_ctor_run;	// [DICE-MOD SwiffPlayer #27] the constructor half of the binding

		// [DICE-MOD SwiffPlayer: getNextHighestDepth's counter (the shipped sprite keeps one at
		// +220, returned then incremented; ours also clears every depth already in use).]
		int	m_next_highest_depth;

		// [DICE-MOD SwiffPlayer: the drawing-API canvas - the shape definition behind the
		// `_____defaultShape` child the shipped sub_1007C0D0 creates at depth 0 on first use.]
		smart_ptr<shape_character_def>	m_canvas;

		// [DICE-MOD SwiffPlayer #45] numeric gotos waiting for advance() (the shipped sprite's
		// pending_goto_frames array at +264, drained by 0x10081CF0).
		array<int>	m_pending_goto_frames;

		// [DICE-MOD SwiffPlayer #51] the shipped sprite's mouse state (+0x2150 previous, +0x2154 current:
		// bit 0 inside, bit 1 a button down), its first-frame latch (+255) and whether it took the
		// press it still holds (+256).
		int	m_mouse_flags, m_prev_mouse_flags;
		bool	m_first_mouse_frame;
		bool	m_mouse_pressed;

		sprite_instance(movie_definition_sub* def, movie_root* r, movie* parent, int id)
			:
			character(parent, id),
			m_def(def),
			m_root(r),
			m_play_state(PLAY),
			m_current_frame(0),
			m_time_remainder(0),
			m_update_frame(true),
			m_has_looped(false),
			m_accept_anim_moves(true),
			m_mouse_state(UP),
			m_mouse_flags(0),
			m_prev_mouse_flags(0),
			m_first_mouse_frame(true),
			m_mouse_pressed(false),
			m_class_bound(false),
			m_class_ctor_run(false),
			m_next_highest_depth(0),
			m_dead(false),	// [DICE-MOD SwiffPlayer #42]
			m_first_frame_actions_pending(false)	// [DICE-MOD SwiffPlayer #53]
		{
			assert(m_def != NULL);
			assert(m_root != NULL);
			
			//m_root->add_ref();	// @@ circular!
			m_as_environment.set_target(this);

			sprite_builtins_init();

			// Initialize the flags for init action executed.
			m_init_actions_executed.resize(m_def->get_frame_count());
			memset(&m_init_actions_executed[0], 0,
			       sizeof(m_init_actions_executed[0]) * m_init_actions_executed.size());
		}

		// sprite instance of add_interval_handler()
		virtual int    add_interval_timer(void *timer)
		{
			// log_msg("FIXME: %s:\n", __FUNCTION__);
			return m_root->add_interval_timer(timer);
		}

		virtual void    clear_interval_timer(int x)
		{
			// log_msg("FIXME: %s:\n", __FUNCTION__);
			m_root->clear_interval_timer(x);
		}
	

		/* sprite_instance */
		virtual void    do_something(void *timer)
		{
			as_value	val;
			as_object_interface      *obj, *this_ptr;   // [DICE-MOD: the tick's this may be a sprite]
			as_environment *as_env;

			//printf("FIXME: %s:\n", __FUNCTION__);
			Timer *ptr = (Timer *)timer;
			//log_msg("INTERVAL ID is %d\n", ptr->getIntervalID());

			const as_value&	timer_method = ptr->getASFunction();
			as_env = ptr->getASEnvironment();
			this_ptr = ptr->getASObject();
			obj = ptr->getObject();
			//m_as_environment.push(obj);
			
			as_c_function_ptr	cfunc = timer_method.to_c_function();
			if (cfunc) {
				// It's a C function. Call it.
				//log_msg("Calling C function for interval timer\n");
				//(*cfunc)(&val, obj, as_env, 0, 0);
				(*cfunc)(fn_call(&val, obj, &m_as_environment, 0, 0));
				
			} else if (as_as_function* as_func = timer_method.to_as_function()) {
				// It's an ActionScript function. Call it.
				as_value method;
				//log_msg("Calling ActionScript function for interval timer\n");
				(*as_func)(fn_call(&val, (as_object_interface *)this_ptr, as_env, 0, 0));
				//(*as_func)(&val, (as_object_interface *)this_ptr, &m_as_environment, 1, 1);
			} else {
				log_error("error in call_method(): method is not a function\n");
			}    
		}	

		bool	m_dead;	// [DICE-MOD SwiffPlayer #42] see character::mark_dead
		virtual void	mark_dead() { m_dead = true; }

		// [DICE-MOD SwiffPlayer #53] frame 0's DoAction tags, queued by on_event_load's
		// execute_frame_tags(0), are NOT run at placement: the shipped on_event_load
		// (sub_10081870) frees the action list (+224) right after the tags, and the script runs
		// when the sprite's first advance re-executes frame 0 - AFTER the parent's own frame
		// script. This flag is that "frame 0's script still owed" state; advance_pass drains it.
		bool	m_first_frame_actions_pending;

		virtual ~sprite_instance()
		{
			// [TEMP] [CHOSEN, not recovered] RW-89: a child still referenced from ActionScript when its
			// parent clip is destroyed (a page torn down by a frame change while a script callback
			// keeps its objects) keeps a RAW m_parent in stock 2006, and the next get_world_matrix()
			// through it faulted retail (interactive run drive1: a component's label field re-laid
			// out from a callback after the login page was left). Until the shipped handling of such
			// survivors is read, name the survivor and hand it the root as parent - it is in no
			// display list, so nothing draws through that matrix. [RETIRE-WHEN: RW-88]
			{
				movie*	root = m_root ? m_root->get_root_movie() : NULL;
				for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
				{
					character*	ch = m_display_list.get_character(i);
					if (ch && ch->get_ref_count() > 1)
					{
						log_error("[orphan] child '%s' of destroyed sprite '%s' survives (refs=%d)" "\n",
							ch->get_name().c_str(), get_name().c_str(), ch->get_ref_count());
						if (root && root != this) ch->m_parent = root;
						ch->mark_dead();
					}
				}
			}
			m_display_list.clear();
			//m_root->drop_ref();
		}

		movie_interface*	get_root_interface() { return m_root; }
		movie_root*	get_root() { return m_root; }
		movie*	get_root_movie() { return m_root->get_root_movie(); }

		movie_definition*	get_movie_definition() { return m_def.get_ptr(); }

		float	get_width()
		{
			float	w = 0;
			int i, n = m_display_list.get_character_count();
			character* ch;
			for (i = 0; i < n; i++)
			{
				ch = m_display_list.get_character(i);
				if (ch != NULL)
				{
					float ch_w = ch->get_width();
					if (ch_w > w)
					{
						w = ch_w;
					}
				}
			}

			return w;
		}



		float	get_height()
		{
			float	h = 0; 
			int i, n = m_display_list.get_character_count();
			character* ch;
			for (i=0; i < n; i++)
			{
				ch = m_display_list.get_character(i);
				if (ch != NULL)
				{
					float	ch_h = ch->get_height();
					if (ch_h > h)
					{
						h = ch_h;
					}
				}
			}
			return h;
		}

		// [DICE-MOD SwiffPlayer #39] the clip's unscaled local extent: the largest child extent, each
		// child measured in this clip's space (its local extent times its own scale). get_width()
		// above carries the WORLD scale and stays for the stock callers.
		virtual float	get_local_width()
		{
			float	w = 0;
			for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
			{
				character*	ch = m_display_list.get_character(i);
				if (ch == NULL) continue;
				const float	cw = ch->get_local_width() * ch->get_matrix().m_[0][0];
				if (cw > w) w = cw;
			}
			return w;
		}
		virtual float	get_local_height()
		{
			float	h = 0;
			for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
			{
				character*	ch = m_display_list.get_character(i);
				if (ch == NULL) continue;
				const float	chh = ch->get_local_height() * ch->get_matrix().m_[1][1];
				if (chh > h) h = chh;
			}
			return h;
		}

		int	get_current_frame() const { return m_current_frame; }
		int	get_frame_count() const { return m_def->get_frame_count(); }

		void	set_play_state(play_state s)
		// Stop or play the sprite.
		{
			if (m_play_state != s)
			{
				m_time_remainder = 0;
			}

			m_play_state = s;
		}
		play_state	get_play_state() const { return m_play_state; }


		character*	get_character(int character_id)
		{
//			return m_def->get_character_def(character_id);
			// @@ TODO -- look through our dlist for a match
			return NULL;
		}

		float	get_background_alpha() const
		{
			// @@ this doesn't seem right...
			return m_root->get_background_alpha();
		}

		float	get_pixel_scale() const { return m_root->get_pixel_scale(); }

		virtual void	get_mouse_state(int* x, int* y, int* buttons)
		{
			m_root->get_mouse_state(x, y, buttons);
		}

		void	set_background_color(const rgba& color)
		{
			m_root->set_background_color(color);
		}

		float	get_timer() const { return m_root->get_timer(); }

		void	restart()
		{
			m_current_frame = 0;
			m_time_remainder = 0;
			m_update_frame = true;
			m_has_looped = false;
			m_play_state = PLAY;

			execute_frame_tags(m_current_frame);
			m_display_list.update();
		}


		virtual bool	has_looped() const { return m_has_looped; }

		virtual bool	get_accept_anim_moves() const { return m_accept_anim_moves; }

		inline int	transition(int a, int b) const
		// Combine the flags to avoid a conditional. It would be faster with a macro.
		{
			return (a << 2) | b;
		}


		// [DICE-MOD SwiffPlayer #51] sub_1007FF90 - does this sprite cover the world-space point: any
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

		// [DICE-MOD SwiffPlayer #51] the root's mouse capture, kept on the movie_root.
		virtual movie*	get_active_mouse_entity() { return m_root->get_active_entity(); }
		virtual void	set_active_mouse_entity(movie* m) { m_root->set_active_entity(m); }

		// [DICE-MOD SwiffPlayer #51] sub_10080010 (709 instructions, read in full 2026-09-13) - the
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
		// The world matrix the shipped body fetches first is never read; it is not fetched here. The
		// [mouse] press/release lines are the RW-89 tracer the interactive drives read [TEMP].
		virtual bool	mouse_update(bool consumed)
		{
			// Keep this (particularly m_as_environment) alive during any method calls!
			smart_ptr<as_object_interface>	this_ptr(this);

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
					const display_object_info&	dobj = m_display_list.get_display_object(i);
					character*	ch = dobj.m_ref ? dobj.m_character.get_ptr() : NULL;
					if (ch != NULL && ch->get_visible())
					{
						const bool was_consumed = consumed;
						consumed = ch->mouse_update(consumed) || consumed;
						if (s_trace_mouse_pass && consumed && !was_consumed)
							log_error("[mouse] consumed by %s id=%d depth=%d\n",
								swf_trace_path(ch), ch->get_id(), ch->get_depth());
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

		/* sprite_instance */
		virtual void	advance(float delta_time)
		{
			// Keep this (particularly m_as_environment) alive during execution!
			smart_ptr<as_object_interface>	this_ptr(this);

			// [DICE-MOD SwiffPlayer #45] The shipped advance (0x10081CF0) first drains the
			// deferred goto queue: each entry is applied, the duplicates right behind it are
			// dropped, and the frame pass runs with a ZERO delta - once per applied goto. The pass
			// runs once with the real delta only when nothing was queued.
			if (m_pending_goto_frames.size() == 0)
			{
				advance_pass(delta_time);
				return;
			}
			do
			{
				const int	target = m_pending_goto_frames[0];
				m_pending_goto_frames.remove(0);
				apply_goto_frame(target);
				while (m_pending_goto_frames.size() > 0 && m_pending_goto_frames[0] == target)
				{
					m_pending_goto_frames.remove(0);
				}
				advance_pass(0.0f);
			}
			while (m_pending_goto_frames.size() > 0);
		}

		// The 2006 frame pass, unchanged but for the #53 drain at its head.
		void	advance_pass(float delta_time)
		{
			assert(m_def != NULL && m_root != NULL);

			// [DICE-MOD SwiffPlayer #53] the first advance owes frame 0's script - see
			// on_event_load. Only the DoAction tags are re-executed (queued in tag order) and the
			// queue drained, before this sprite's own children advance: the shipped first pass
			// is execute_frame_tags(0) -> onEnterFrame -> do_actions -> display_list::advance.
			if (m_first_frame_actions_pending)
			{
				m_first_frame_actions_pending = false;
				const array<execute_tag*>&	playlist = m_def->get_playlist(0);
				for (int i = 0; i < playlist.size(); i++)
				{
					execute_tag*	e = playlist[i];
					if (e->is_action_tag())
					{
						e->execute(this);
					}
				}
				do_actions();
			}

			// Advance everything in the display list.
			m_display_list.advance(delta_time);

			// mouse drag.
			character::do_mouse_drag();

			m_time_remainder += delta_time;

			const float	frame_time = 1.0f / m_root->get_frame_rate();	// @@ cache this

			// Check for the end of frame
			if (m_time_remainder >= frame_time)
			{
				m_time_remainder -= frame_time;

				// Update current and next frames.
				if (m_play_state == PLAY)
				{
					int	current_frame0 = m_current_frame;
					increment_frame_and_check_for_loop();

					// Execute the current frame's tags.
					if (m_current_frame != current_frame0)
					{
						execute_frame_tags(m_current_frame);
					}
				}

				// Dispatch onEnterFrame event.
				on_event(event_id::ENTER_FRAME);

				do_actions();

				// Clean up display list (remove dead objects).
				m_display_list.update();
			}

			// Skip excess time.  TODO root caller should
			// loop to prevent this happening; probably
			// only root should keep m_time_remainder, and
			// advance(dt) should be a discrete tick()
			// with no dt.
			m_time_remainder = fmod(m_time_remainder, frame_time);
		}

		/*sprite_instance*/
		void	execute_frame_tags(int frame, bool state_only = false)
		// Execute the tags associated with the specified frame.
		// frame is 0-based
		{
			// Keep this (particularly m_as_environment) alive during execution!
			smart_ptr<as_object_interface>	this_ptr(this);

			assert(frame >= 0);
			assert(frame < m_def->get_frame_count());

			// Execute this frame's init actions, if necessary.
			if (m_init_actions_executed[frame] == false)
			{
				const array<execute_tag*>*	init_actions = m_def->get_init_actions(frame);
				if (init_actions && init_actions->size() > 0)
				{
					// Need to execute these actions.
					for (int i= 0; i < init_actions->size(); i++)
					{
						execute_tag*	e = (*init_actions)[i];
						e->execute(this);
					}

					// [DICE-MOD SwiffPlayer: run them NOW. A DoInitAction is loaded as a plain
					// do_action, whose execute() only QUEUES its buffer for do_actions() - so stock
					// 2006 ran every "init" action after this frame's placements, in the same batch
					// as the frame script. Flash runs init actions before the frame's other tags,
					// and the shipped load path (sub_10081870) executes the pending init actions
					// before execute_frame_tags(0). mainMenu.swf depends on it: frame 0 places its
					// components first and carries the 52 class definitions + 18
					// Object.registerClass calls as DoInitAction tags after them, so on stock order
					// every component was instanced before its class existed (retail 2026-09-02:
					// "can't find method loadImg/setValue" ahead of the registerClass calls).]
					do_actions();

					// Mark this frame done, so we never execute these init actions
					// again.
					m_init_actions_executed[frame] = true;
				}
			}

			const array<execute_tag*>&	playlist = m_def->get_playlist(frame);
			for (int i = 0; i < playlist.size(); i++)
			{
				execute_tag*	e = playlist[i];
				if (state_only)
				{
					e->execute_state(this);
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

			// [DICE-MOD SwiffPlayer #48] The shipped do_actions (sub_10081480, the loop at
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
		}


		/*sprite_instance*/
		void	goto_frame(int target_frame_number)
		// Set the sprite state at the specified frame number.
		// 0-based frame numbers!!  (in contrast to ActionScript and Flash MX)
		{
			// [DICE-MOD SwiffPlayer #45] A numeric goto is DEFERRED. The shipped goto_frame (sprite
			// vtable +168, sub_10081310) only appends the target to a per-sprite queue; advance()
			// (0x10081CF0) drains it before its frame pass, applying each entry through
			// apply_goto_frame (vtable +24) with the duplicates right behind it dropped, and runs
			// the pass with a zero delta. The label form (goto_labeled_frame, vtable +28) applies
			// at once. Stock 2006 replayed the timeline inline here.
			m_pending_goto_frames.push_back(target_frame_number);

			// goto_frame stops by default.
			m_play_state = STOP;
		}

		// [DICE-MOD SwiffPlayer #45] The depth -> [placing tag, move tags...] state in effect after
		// frames 0..frame inclusive, as the shipped sub_100832F0 builds it: PLACE and REPLACE start
		// a depth's list, MOVE appends to an existing one (a move with no placement is dropped) and
		// REMOVE leaves an EMPTY list, so a removed depth is distinguishable from an unused one.
		void	build_timeline_state(int frame, hash<int, array<execute_tag*> >* out)
		{
			frame = iclamp(frame, 0, m_def->get_frame_count() - 1);
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

		// [DICE-MOD SwiffPlayer #45] The shipped goto (sprite vtable +24, sub_100822A0) is a
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
		// (RETRIEVE ACCOUNT -> MANAGE ACCOUNTS lost its rows). The queued lists run in depth order;
		// the shipped walks its own hash [CHOSEN, not recovered - unobservable].
		void	apply_goto_frame(int target_frame_number)
		{
			// Keep this (particularly m_as_environment) alive during execution!
			smart_ptr<as_object_interface>	this_ptr(this);

			// [DICE-MOD SwiffPlayer #53] a goto applied before the first pass takes the owed
			// frame-0 script with it: the target frame's actions are what runs from here on.
			const bool	frame0_owed = m_first_frame_actions_pending;
			m_first_frame_actions_pending = false;

			target_frame_number = iclamp(target_frame_number, 0, m_def->get_frame_count() - 1);
			// [TEMP] RW-88 nav tracer: which clip navigates where, and from what.
			log_error("[goto_frame] '%s' %d -> %d (of %d) dlist=%d\n", swf_trace_path(this),
			          m_current_frame, target_frame_number, m_def->get_frame_count(),
			          m_display_list.get_character_count());

			if (target_frame_number == m_current_frame)
			{
				// [DICE-MOD SwiffPlayer #56] The shipped same-frame arm (sub_100822A0's third
				// branch): a goto to frame 0 while frame 0 has never run (+258
				// first_frame_initialized clear) executes frame 0's tags, marks it run, and takes
				// the common tail - current 0 / next 1, the time remainder and the update flag
				// cleared, the sweep, ENTER_FRAME, the queued actions. Any other same-frame goto
				// returns with nothing done. Under #53 a placed sprite's frame 0 has NOT run until
				// its first advance, so this arm is live here: mainMenu.swf's `gotoPage` opens
				// with `clearModalMessageTrigger()` -> `messageHandler.gotoAndStop("noMessage")`
				// (frame 0) before the root-placed messageHandler's first advance. Dropping the
				// owed script instead left `checkForMessages` undefined, the UpdateManager's first
				// poll missed it ("can't find method checkForMessages"), unregistered the falsy
				// callback, and no host message reached the screen for the rest of the session -
				// the CREATE LOCAL page's STARTING SERVER modal stayed up over a "nomapsselected"
				// the owner never saw (2026-09-29). As in #53's drain, only frame 0's ACTION tags
				// are re-run: the shipped replay of its placements finds their live
				// on_event_load occupants (same id and name, still referenced) and does nothing
				// (sub_100837E0 - see the #44/#53 correction in GAMESWF_DIVERGENCES.md).
				if (target_frame_number != 0 || !frame0_owed)
				{
					return;
				}
				const array<execute_tag*>&	playlist = m_def->get_playlist(0);
				for (int i = 0; i < playlist.size(); i++)
				{
					execute_tag*	e = playlist[i];
					if (e->is_action_tag())
					{
						e->execute(this);
					}
				}
				m_current_frame = 0;
				m_time_remainder = 0;
				m_update_frame = false;
				m_display_list.update();
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

			array<int>	depths;
			for (hash<int, array<execute_tag*> >::iterator it = target_state.begin(); it != target_state.end(); ++it)
			{
				const int	d = it->first;
				int	pos = depths.size();
				depths.push_back(d);
				while (pos > 0 && depths[pos - 1] > d)
				{
					depths[pos] = depths[pos - 1];
					pos--;
				}
				depths[pos] = d;
			}

			array< array<execute_tag*> >	queued;
			for (int di = 0; di < depths.size(); di++)
			{
				const int	depth = depths[di];
				array<execute_tag*>	list;
				target_state.get(depth, &list);
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
					if (ch && placing->is_place_tag())
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
					m_display_list.remove_display_object((Uint16) it->first, -1);
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
				const array<execute_tag*>&	playlist = m_def->get_playlist(target_frame_number);
				for (int i = 0; i < playlist.size(); i++)
				{
					playlist[i]->execute_goto(this);
				}
			}

			m_current_frame = target_frame_number;
			m_time_remainder = 0;
			m_update_frame = false;
			m_display_list.update();
			on_event(event_id::ENTER_FRAME);
			do_actions();
		}


		bool	goto_labeled_frame(const char* label)
		// Look up the labeled frame, and jump to it.
		{
			int	target_frame = -1;
			if (m_def->get_labeled_frame(label, &target_frame))
			{
				// [DICE-MOD SwiffPlayer #45] applied at once, as the shipped +28 (0x100802E0)
				// dispatches straight to +24; the 2006 stop-by-default is kept for the callers
				// that rely on it (the AS methods set their own state right after).
				apply_goto_frame(target_frame);
				m_play_state = STOP;
				return true;
			}
			else
			{
				IF_VERBOSE_ACTION(
					log_error("error: movie_impl::goto_labeled_frame('%s') unknown label\n", label));
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

			do_display_callback();
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

			// [DICE-MOD SwiffPlayer #44] No "same character already on this plane -> move it
			// instead" shortcut. Stock 2006 turned a PLACE whose depth already held the same id (and
			// name) into a move_display_object, which re-references the occupant. Flash MX 2004
			// encodes a new keyframe instance of the same symbol as RemoveObject2 + PlaceObject2 on
			// one depth in one frame (mainMenu.swf: 337 such sites, none placing over a live
			// occupant; e.g. the login container, sprite 402, frame 29 "manage": depths 3
			// profilesList, 46, 48), so the shortcut resurrected the instance the same frame had
			// just removed - unmoved, because a script-touched clip rejects anim moves - and the
			// login page's list and buttons stayed drawn under the Account Management page. The
			// shipped sprite::add_display_object (sub_1007BA80) instantiates unconditionally, and
			// its display_list::add_display_object (sub_100A8080) unreferences whatever holds the
			// depth (drops the focus if it was inside it, nulls its parent) before inserting; neither
			// compares ids or names.

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

		// [DICE-MOD SwiffPlayer: add an instance of an ARBITRARY definition (an exported symbol for
		// attachMovie, a fresh anonymous sprite for createEmptyMovieClip) - the tail of
		// add_display_object without the character-id lookup. The shipped player's own
		// add_display_object (sub_1007BA80) takes the definition pointer directly.]
		character*	add_display_object_def(
			character_def* cdef,
			const char* name,
			Uint16 depth)
		{
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
				0.0f,
				0);
			return ch.get_ptr();
		}

		// [DICE-MOD SwiffPlayer: the highest depth in use, -1 when empty - for getNextHighestDepth.]
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

		// [DICE-MOD SwiffPlayer: the shipped getNextHighestDepth/loadMovie counter semantics -
		// return the current value, bump past everything in use.]
		int	take_next_highest_depth()
		{
			int	d = m_next_highest_depth;
			const int	in_use = get_highest_depth_in_use() + 1;
			if (d < in_use)
			{
				d = in_use;
			}
			m_next_highest_depth = d + 1;
			return d;
		}

		// [DICE-MOD SwiffPlayer: the drawing-API canvas (sub_1007C0D0): a `_____defaultShape` child
		// at depth 0 over a fresh shape definition, created on first use, mutated by
		// lineStyle/moveTo/lineTo/beginFill/endFill.]
		shape_character_def*	get_canvas_def()
		{
			if (m_canvas == NULL)
			{
				m_canvas = new shape_character_def;
				add_display_object_def(m_canvas.get_ptr(), "_____defaultShape", 0);
			}
			return m_canvas.get_ptr();
		}

		// [DICE-MOD SwiffPlayer: remove every child - the first thing the shipped loadMovie does.]
		void	remove_all_display_objects()
		{
			array<int>	depths;
			for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
			{
				character*	ch = m_display_list.get_character(i);
				if (ch)
				{
					depths.push_back(ch->get_depth());
				}
			}
			for (int i = 0; i < depths.size(); i++)
			{
				m_display_list.remove_display_object((Uint16) depths[i], -1);
			}
			m_canvas = NULL;
		}


		/*sprite_instance*/
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
		void	remove_display_object(Uint16 depth, int id)
		// Remove the object at the specified depth.
		// If id != -1, then only remove the object at depth with matching id.
		{
			m_display_list.remove_display_object(depth, id);
		}


		/*sprite_instance*/
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
		virtual void	set_variable(const char* path_to_var, const char* new_value)
		{
			assert(m_parent == NULL);	// should only be called on the root movie.

			if (path_to_var == NULL)
			{
				log_error("error: NULL path_to_var passed to set_variable()\n");
				return;
			}
			if (new_value == NULL)
			{
				log_error("error: NULL passed to set_variable('%s', NULL)\n", path_to_var);
				return;
			}

			array<with_stack_entry>	empty_with_stack;
			tu_string	path(path_to_var);
			as_value	val(new_value);

			m_as_environment.set_variable(path, val, empty_with_stack);
		}

		/* sprite_instance */
		virtual void	set_variable(const char* path_to_var, const wchar_t* new_value)
		{
			if (path_to_var == NULL)
			{
				log_error("error: NULL path_to_var passed to set_variable()\n");
				return;
			}
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
		// Set the named member to the value.  Return true if we have
		// that member; false otherwise.
		{
			if (m_dead) return;	// [DICE-MOD SwiffPlayer #42]
			// [DICE-MOD SwiffPlayer #51] sub_10083A20's first act: an "on<Capital>" member whose name is
			// one of the eleven mouse handlers marks the sprite mouse-enabled (its name table, built once:
			// onDragOut, onDragOver, onMouseDown, onMouseMove, onMouseUp, onPress, onDoublePress,
			// onRelease, onReleaseOutside, onRollOut, onRollOver). The value is not inspected.
			if (!m_mouse_enabled)
			{
				const char*	s = name.c_str();
				if (s[0] == 'o' && s[1] == 'n' && s[2] >= 'A' && s[2] <= 'Z')
				{
					static const char*	MOUSE_HANDLER_NAMES[] = {
						"onDragOut", "onDragOver", "onMouseDown", "onMouseMove", "onMouseUp", "onPress",
						"onDoublePress", "onRelease", "onReleaseOutside", "onRollOut", "onRollOver",
					};
					for (int i = 0; i < ARRAYSIZE(MOUSE_HANDLER_NAMES); i++)
					{
						if (strcmp(s, MOUSE_HANDLER_NAMES[i]) == 0)
						{
							m_mouse_enabled = true;
							break;
						}
					}
				}
			}
			as_standard_member	std_member = get_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_X:
				//if (name == "_x")
			{
				matrix	m = get_matrix();
				m.m_[0][2] = (float) PIXELS_TO_TWIPS(val.to_number());
				set_matrix(m);

				m_accept_anim_moves = false;

				return;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();
				m.m_[1][2] = (float) PIXELS_TO_TWIPS(val.to_number());
				set_matrix(m);

				m_accept_anim_moves = false;

				return;
			}
			case M_XSCALE:
				//else if (name == "_xscale")
			{
				matrix	m = get_matrix();

				// Decompose matrix and insert the desired value.
				float	x_scale = (float) val.to_number() / 100.f;	// input is in percent
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
				float	y_scale = (float) val.to_number() / 100.f;	// input is in percent
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
				cx.m_[3][0] = float(val.to_number()) / 100.f;
				set_cxform(cx);
				m_accept_anim_moves = false;
				return;
			}
			case M_VISIBLE:
				//else if (name == "_visible")
			{
				log_error("[_visible] '%s' = %d\n", swf_trace_path(this), val.to_bool() ? 1 : 0);  // [TEMP] RW-88
				set_visible(val.to_bool());
				m_accept_anim_moves = false;
				return;
			}
			case M_WIDTH:
				//else if (name == "_width")
			{
				// [DICE-MOD SwiffPlayer #39] Flash: own x-scale = value / the clip's unscaled local extent
				// (absolute). Stock divided PIXELS_TO_TWIPS(value) by the WORLD-scaled get_width(), which
				// compounded every set by the scale in play: the DICE components read `_width` back and
				// hand it to their label fields, so the text layout widths came out up to 3.3x too wide
				// (retail run Z vs the golden: version field 208 vs 156, LOGIN 1286 vs 388). A clip
				// without extent keeps its scale (stock left it PIXELS_TO_TWIPS(value) - the 1200x
				// image clip). See GAMESWF_DIVERGENCES.md #39.
				const float	local = get_local_width();
				if (fabsf(local) > 1e-6f)
				{
					matrix	m = get_matrix();
					m.m_[0][0] = float(PIXELS_TO_TWIPS(val.to_number())) / local;
					set_matrix(m);
				}
				m_accept_anim_moves = false;
				return;
			}
			case M_HEIGHT:
				//else if (name == "_height")
			{
				// [DICE-MOD SwiffPlayer #39] as M_WIDTH (stock also divided by get_WIDTH here).
				const float	local = get_local_height();
				if (fabsf(local) > 1e-6f)
				{
					matrix	m = get_matrix();
					m.m_[1][1] = float(PIXELS_TO_TWIPS(val.to_number())) / local;
					set_matrix(m);
				}
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
				float	rotation = (float) val.to_number() * float(M_PI) / 180.f;	// input is in degrees
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

			// Not a built-in property.  See if we have a
			// matching edit_text character in our display
			// list.
			bool	text_val = val.get_type() == as_value::STRING
				|| val.get_type() == as_value::NUMBER;
			if (text_val)
			{
				bool	success = false;
				for (int i = 0, n = m_display_list.get_character_count(); i < n; i++)
				{
					character*	ch = m_display_list.get_character(i);
					// CASE INSENSITIVE compare.  In ActionScript 2.0, this
					// changes to CASE SENSITIVE!!!
					if (ch && name == ch->get_text_name())
					{
						const char* text = val.to_string();
						ch->set_text_value(text);
						success = true;
					}
				}
				if (success) return;
			}

			// [DICE-MOD SwiffPlayer #28: a setter on the registered class's prototype chain runs with
			// this sprite as `this` - the construct handlers' `__authorMode = false` lands in
			// UIObject's __set____authorMode, which stores m_inAuthorMode on the instance.]
			if (m_class_proto != NULL)
			{
				as_object*	proto = m_class_proto->cast_to_as_object();
				as_value	setter;
				if (proto && proto->find_setter(name, &setter))
				{
					as_object::invoke_setter(setter, this, val);
					return;
				}
			}
			// If that didn't work, set a variable within this environment.
			m_as_environment.set_member(name, val);
		}


		/* sprite_instance */
		virtual bool	get_member(const tu_stringi& name, as_value* val)
		// Set *val to the value of the named member and
		// return true, if we have the named member.
		// Otherwise leave *val alone and return false.
		{
			if (m_dead) return false;	// [DICE-MOD SwiffPlayer #42] a removed clip answers undefined
			as_standard_member	std_member = get_standard_member(name);
			switch (std_member)
			{
			default:
			case M_INVALID_MEMBER:
				break;
			case M_X:
				//if (name == "_x")
			{
				matrix	m = get_matrix();
				val->set_double(TWIPS_TO_PIXELS(m.m_[0][2]));
				return true;
			}
			case M_Y:
				//else if (name == "_y")
			{
				matrix	m = get_matrix();
				val->set_double(TWIPS_TO_PIXELS(m.m_[1][2]));
				return true;
			}
			case M_XSCALE:
				//else if (name == "_xscale")
			{
				matrix m = get_matrix();	// @@ or get_world_matrix()?  Test this.
				float xscale = m.get_x_scale();
				val->set_double(xscale * 100);		// result in percent
				return true;
			}
			case M_YSCALE:
				//else if (name == "_yscale")
			{
				matrix m = get_matrix();	// @@ or get_world_matrix()?  Test this.
				float yscale = m.get_y_scale();
				val->set_double(yscale * 100);		// result in percent
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
				val->set_double(get_cxform().m_[3][0] * 100.f);
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
				// [DICE-MOD SwiffPlayer #39] Flash: the clip's unscaled local extent times its own
				// x-scale, in pixels - independent of ancestors. Stock measured through the world
				// matrix twice (get_width() carries it, enclose_transformed_rect applied it again).
				val->set_double(TWIPS_TO_PIXELS(get_local_width() * get_matrix().m_[0][0]));
				return true;
			}
			case M_HEIGHT:
				//else if (name == "_height")
			{
				val->set_double(TWIPS_TO_PIXELS(get_local_height() * get_matrix().m_[1][1]));
				return true;
			}
			case M_ROTATION:
				//else if (name == "_rotation")
			{
				// Verified against Macromedia player using samples/test_rotation.swf
				float	angle = get_matrix().get_rotation();

				// Result is CLOCKWISE DEGREES, [-180,180]
				angle *= 180.0f / float(M_PI);

				val->set_double(angle);
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

				point	a(PIXELS_TO_TWIPS(x), PIXELS_TO_TWIPS(y));
				point	b;
				
				m.transform_by_inverse(&b, a);

				val->set_double(TWIPS_TO_PIXELS(b.m_x));
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

				point	a(PIXELS_TO_TWIPS(x), PIXELS_TO_TWIPS(y));
				point	b;
				
				m.transform_by_inverse(&b, a);

				val->set_double(TWIPS_TO_PIXELS(b.m_y));
				return true;
			}
			case M_PARENT:
			{
				val->set_as_object_interface(static_cast<as_object_interface*>(m_parent));
				return true;
			}
			case M_ONLOAD:
			{
				if (m_as_environment.get_member(name, val))
				{
					return true;
				}
				// Optimization: if no hit, don't bother looking in the display list, etc.
				return false;
			}
			}	// end switch

			// Try variables.
			if (m_as_environment.get_member(name, val))
			{
				return true;
			}

			// [DICE-MOD SwiffPlayer: the registered class's prototype chain, after the instance's
			// own members and before children/builtins - the shipped sprite object resolves its own
			// members, then `prototype`, then `__proto__` (sub_10099FE0).]
			if (m_class_proto != NULL)
			{
				if (name == "__proto__")
				{
					val->set_as_object_interface(m_class_proto.get_ptr());
					return true;
				}
				// [DICE-MOD SwiffPlayer #28: a getter on the class prototype chain, with this sprite
				// as `this` (a plain prototype lookup would run it with the prototype as `this`).]
				{
					as_object*	proto = m_class_proto->cast_to_as_object();
					as_value	getter;
					if (proto && proto->find_getter(name, &getter))
					{
						*val = as_object::invoke_getter(getter, this);
						return true;
					}
				}
				if (m_class_proto->get_member(name, val))
				{
					return true;
				}
			}

			// Not a built-in property.  Check items on our
			// display list.
			// [DICE-MOD SwiffPlayer: CASE-SENSITIVE child lookup. Stock 2006 used the _i (case-
			// insensitive) variant; the shipped sprite get_member (sub_10080590) walks the display
			// list with strcmp, which is the SWF 7 rule BF2's menus (version 7) are written to. With
			// the insensitive lookup a child instance named "color" shadowed the Color class, so
			// `new Color(mc)` got a movie clip back (retail 2026-09-02, probe: "lookup type=OBJECT (a
			// movie clip); _global.Color present"). See GAMESWF_DIVERGENCES.md.]
			character*	ch = m_display_list.get_character_by_name(tu_string(name.c_str()));
			if (ch)
			{
				// Found object.
				val->set_as_object_interface(static_cast<as_object_interface*>(ch));
				return true;
			}

			// Try static builtin functions.
			assert(s_sprite_builtins);
			if (s_sprite_builtins->get_member(name, val))
			{
				return true;
			}

			return false;
		}


		/* sprite_instance */
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
					frame_number = (int) frame_spec.to_number();
				}
			}
			else
			{
				// convert from 1-based to 0-based
				frame_number = (int) frame_spec.to_number() - 1;
			}

			if (frame_number < 0 || frame_number >= m_def->get_frame_count())
			{
				// No dice.
				log_error("error: call_frame('%s') -- unknown frame\n", frame_spec.to_string());
				return;
			}

			int	top_action = m_action_list.size();

			// Execute the actions.
			const array<execute_tag*>&	playlist = m_def->get_playlist(frame_number);
			for (int i = 0; i < playlist.size(); i++)
			{
				execute_tag*	e = playlist[i];
				if (e->is_action_tag())
				{
					e->execute(this);
				}
			}

			// Execute any new actions triggered by the tag,
			// leaving existing actions to be executed.
			while (m_action_list.size() > top_action)
			{
				m_action_list[top_action]->execute(&m_as_environment);
				m_action_list.remove(top_action);
			}
			assert(m_action_list.size() == top_action);
		}


		/* sprite_instance */
		virtual void	set_drag_state(const drag_state& st)
		{
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


		void	remove_display_object(const tu_string& name)
		// Remove the object with the specified name.
		{
			character* ch = m_display_list.get_character_by_name(name);
			if (ch)
			{
				// @@ TODO: should only remove movies that were created via clone_display_object --
				// apparently original movies, placed by anim events, are immune to this.
				remove_display_object(ch->get_depth(), ch->get_id());
			}
		}

		
		/* sprite_instance */
		virtual bool	on_event(event_id id)
		// Dispatch event handler(s), if any.
		{
			// Keep m_as_environment alive during any method calls!
			smart_ptr<as_object_interface>	this_ptr(this);

			bool called = false;

			// [DICE-MOD SwiffPlayer #51] sub_10081620: the clip-event handler when there is one, ELSE
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
			return called;
		}


		/*sprite_instance*/
		virtual void	on_event_load()
		// Do the events that (appear to) happen as the movie
		// loads.  frame1 tags and actions are executed (even
		// before advance() is called).  Then the onLoad event
		// is triggered.
		{
			execute_frame_tags(0);
			// [DICE-MOD SwiffPlayer #53] The shipped on_event_load (sub_10081870, sprite vtable
			// slot 51, called by display_list::add_display_object right after the insert) runs
			// the pending init actions, executes frame 0's tags (sub_10081B10 - the placements
			// happen now) and then FREES the action list at +224: the frame-0 DoAction buffers
			// that execute_frame_tags just queued are dropped. The script runs when the sprite's
			// first advance (0x10081CF0: next_frame 0, `first_frame_initialized` false) executes
			// frame 0 again and drains the queue - and that advance comes AFTER the placing
			// parent's own frame script, because the shipped pass advances its display list
			// (sub_100A82A0) after its execute_frame_tags / do_actions. Stock 2006 drained the
			// queue right here, so a child's frame-0 script ran BEFORE the parent's frame script
			// of the same frame. mainMenu.swf's JOIN INTERNET page depends on the shipped order:
			// the page's frame script defines getIsBrowsed / setIsBrowsed and the refresh modal
			// placed on that frame calls them from its own frame-0 script; on the stock order
			// they were "can't find method", the modal's setIsBrowsed was lost, and the modal
			// started a SECOND browse when its script ran again (the owner's "server list loads
			// twice", 2026-09-14). [CHOSEN, not recovered]: the shipped re-executes frame 0's
			// PLACEMENTS too on that first advance (replacing the instances made here); this port
			// keeps the instances and re-runs only the DoAction tags - see advance_pass.
			m_action_list.clear();
			m_first_frame_actions_pending = true;
			// [DICE-MOD SwiffPlayer #27: after the frame-0 tags, before onLoad - prototype, the
			// construct handlers (component parameters), the class constructor, initialize, load.]
			bind_class_prototype();
			on_event(event_id::CONSTRUCT);
			run_class_constructor();
			on_event(event_id::INITIALIZE);
			on_event(event_id::LOAD);
		}

		// [DICE-MOD SwiffPlayer: Object.registerClass support. The shipped load path
		// (sub_10081870) does, for an instance whose definition carries a registered class:
		//   instance.__proto__       = Class.prototype   (the function's +40 prototype object)
		//   instance.__constructor__ = Class
		//   Class.call(instance)                        (the constructor runs with the sprite as
		//                                                 `this`, in the sprite's own environment)
		// once, right after the frame-0 tags. Stock 2006 sprites have no prototype chain at all, so
		// every attached UI component stayed a bare MovieClip (retail 2026-09-02: "can't find
		// method loadImg/setValue/registerCallback" on every menu component, timeline parked).]
		// [DICE-MOD SwiffPlayer #27] The binding comes in two halves. The prototype is attached
		// first, so the `construct` clip handlers (component-inspector parameters such as
		// `__authorMode = false`) reach the class's addProperty setters; the constructor runs after
		// them, as Flash 7 orders it (construct -> constructor -> initialize -> load).
		void	bind_class_prototype()
		{
			if (m_class_bound || m_def == NULL || m_def->m_registered_class == NULL)
			{
				return;
			}
			m_class_bound = true;

			as_as_function*	cls = static_cast<as_as_function*>(m_def->m_registered_class);
			cls->lazy_create_properties();
			as_value	proto;
			if (cls->m_properties)
			{
				cls->m_properties->get_member("prototype", &proto);
			}
			m_class_proto = proto.to_object();
			m_as_environment.set_member("__constructor__", as_value(cls));
		}

		void	run_class_constructor()
		{
			if (m_class_ctor_run || !m_class_bound || m_def == NULL || m_def->m_registered_class == NULL)
			{
				return;
			}
			m_class_ctor_run = true;
			as_value	ctor(static_cast<as_as_function*>(m_def->m_registered_class));
			call_method0(ctor, &m_as_environment, this);
		}

		void	bind_registered_class()
		{
			bind_class_prototype();
			run_class_constructor();
		}

		// Do the events that happen when there is XML data waiting
		// on the XML socket connection.
		virtual void	on_event_xmlsocket_onxml()
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
			on_event(event_id::SOCK_XML);
		}
		
		// Do the events that (appear to) happen on a specified interval.
		virtual void	on_event_interval_timer()
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
			on_event(event_id::TIMER);
		}

		// Do the events that happen as a MovieClip (swf 7 only) loads.
		virtual void	on_event_load_progress()
		{
			log_msg("FIXME: %s: unimplemented\n", __FUNCTION__);
			on_event(event_id::LOAD_PROGRESS);
		}

	/*sprite_instance*/
		virtual const char*	call_method_args(const char* method_name, const char* method_arg_fmt, va_list args)
		{
			// Keep m_as_environment alive during any method calls!
			smart_ptr<as_object_interface>	this_ptr(this);

			return call_method_parsed(&m_as_environment, this, method_name, method_arg_fmt, args);
		}

		/* sprite_instance */
		virtual void	attach_display_callback(const char* path_to_object, void (*callback)(void*), void* user_ptr)
		{
			assert(m_parent == NULL);	// should only be called on the root movie.

			array<with_stack_entry>	dummy;
			as_value	obj = m_as_environment.get_variable(tu_string(path_to_object), dummy);
			as_object_interface*	as_obj = obj.to_object();
			if (as_obj)
			{
				movie*	m = as_obj->to_movie();
				if (m)
				{
					m->set_display_callback(callback, user_ptr);
				}
			}
		}
	};


	character*	sprite_definition::create_character_instance(movie* parent, int id)
	// Create a (mutable) instance of our definition.  The
	// instance is created to live (temporarily) on some level on
	// the parent movie's display list.
	{
		sprite_instance*	si = new sprite_instance(this, parent->get_root(), parent, id);

		return si;
	}


	movie_interface*	movie_def_impl::create_instance()
	// Create a playable movie instance from a def.
	{
		movie_root*	m = new movie_root(this);
		assert(m);

		sprite_instance*	root_movie = new sprite_instance(this, m, NULL, -1);
		assert(root_movie);

		root_movie->set_name("_root");
		m->set_root_movie(root_movie);

		m->add_ref();
		return m;
	}


	void	sprite_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Create and initialize a sprite, and add it to the movie.
	{
		assert(tag_type == 39);
                
		int	character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  sprite\n  char id = %d\n", character_id));

		sprite_definition*	ch = new sprite_definition(m);	// @@ combine sprite_definition with movie_def_impl
		ch->read(in);

		m->add_character(character_id, ch);
	}


	//
	// sprite built-in ActionScript methods
	//

	// =============================================================================================
	// [DICE-MOD SwiffPlayer: the MovieClip methods the shipped sprite builtins object
	// (gameswf_standard_method_map_init, dword_100F8C98) carries and stock 2006 lacks. BF2's menu
	// class framework builds every component with them. Each is reconstructed from the named
	// shipped body:
	//   createEmptyMovieClip(name, depth)  gameswf_sprite_create_empty_movieclip - a fresh sprite
	//                                      definition on the root movie's definition, instanced at
	//                                      `depth` under `name`; two arguments exactly.
	//   attachMovie(symbol, name, depth)   gameswf_sprite_attach_movie - the root definition's
	//                                      exported symbol, instanced at `depth`; three or more args.
	//   getNextHighestDepth()              gameswf_sprite_getNextHighestDepth - a per-sprite counter
	//                                      returned then incremented (+220); ours also skips depths
	//                                      the timeline already occupies.
	//   removeMovieClip()                  gameswf_sprite_remove_movieclip - parent->remove(depth).
	//   setMask(mc)                        gameswf_sprite_set_mask - mask stored on the character,
	//                                      mask flagged mask-only (+192/+196); one argument.
	// See GAMESWF_DIVERGENCES.md.]
	// =============================================================================================
	static sprite_instance*	sprite_this(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
			// [TEMP] RW-89 target trace: a builtin reached without `this` binds to the env target.
			log_error("[sprite_this] this_ptr NULL -> env target '%s'\n", sprite ? sprite->get_name().c_str() : "<none>");
		}
		return sprite;
	}

	void	sprite_create_empty_movieclip(const fn_call& fn)
	{
		if (fn.nargs != 2)
		{
			log_error("error: create_empty_movieclip needs two args\n");
			return;
		}
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		const tu_string	name = fn.arg(0).to_tu_string();
		const int	depth = (int) fn.arg(1).to_number();

		movie*	root = sprite->get_root_movie();
		movie_definition_sub*	root_def = root
			? static_cast<movie_definition_sub*>(root->get_movie_definition()) : NULL;
		if (root_def == NULL)
		{
			fn.result->set_as_object_interface(NULL);
			return;
		}
		// A one-frame, empty sprite definition owned by the instance (its m_def smart_ptr).
		sprite_definition*	def = new sprite_definition(root_def);
		def->m_frame_count = 1;
		def->m_playlist.resize(1);

		character*	ch = sprite->add_display_object_def(def, name.c_str(), (Uint16) depth);
		// [TEMP] RW-89 target trace. Remove with the tracers.
		log_error("[createEmptyMovieClip] on '%s' -> '%s' depth=%d ch=%p\n", sprite->get_name().c_str(), name.c_str(), depth, ch);
		fn.result->set_as_object_interface(ch);
	}

	void	sprite_attach_movie(const fn_call& fn)
	{
		if (fn.nargs < 3)
		{
			log_error("error: attach_movie needs atleast three args\n");
			return;
		}
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		const tu_string	symbol = fn.arg(0).to_tu_string();
		const tu_string	name = fn.arg(1).to_tu_string();
		const int	depth = (int) fn.arg(2).to_number();

		movie*	root = sprite->get_root_movie();
		movie_definition_sub*	root_def = root
			? static_cast<movie_definition_sub*>(root->get_movie_definition()) : NULL;
		smart_ptr<resource>	res = root_def ? root_def->get_exported_resource(symbol) : smart_ptr<resource>();
		character_def*	cdef = res.get_ptr() ? res->cast_to_character_def() : NULL;
		if (cdef == NULL)
		{
			log_error("error: attachMovie: no exported symbol '%s'\n", symbol.c_str());
			*fn.result = as_value();
			return;
		}
		character*	ch = sprite->add_display_object_def(cdef, name.c_str(), (Uint16) depth);
		// [TEMP] RW-89 target trace. Remove with the tracers.
		log_error("[attachMovie] on '%s' symbol='%s' -> '%s' depth=%d ch=%p\n", swf_trace_path(sprite), symbol.c_str(), name.c_str(), depth, ch);
		fn.result->set_as_object_interface(ch);
	}

	void	sprite_get_next_highest_depth(const fn_call& fn)
	{
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL)
		{
			return;
		}
		int	d = sprite->m_next_highest_depth;
		const int	in_use = sprite->get_highest_depth_in_use() + 1;
		if (d < in_use)
		{
			d = in_use;
		}
		sprite->m_next_highest_depth = d + 1;
		fn.result->set_int(d);
	}

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
		if (parent)
		{
			// Keep the instance alive across its own removal - the caller may still be running
			// one of its methods.
			smart_ptr<as_object_interface>	keep(sprite);
			parent->remove_display_object((Uint16) sprite->get_depth(), -1);
		}
	}

	void	sprite_set_mask(const fn_call& fn)
	{
		if (fn.nargs < 1)
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
		sprite->m_mask = mask;
		if (mask)
		{
			mask->m_is_mask_only = true;
		}
	}

	// =============================================================================================
	// [DICE-MOD SwiffPlayer: the DRAWING API and the IMAGE LOADER of the shipped sprite builtins,
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
	//   beginGradientFill(...)             gameswf_sprite_begin_gradient_fill - NOT transcribed
	//                                      (a 1.4 KB gradient-matrix builder); logged, falls back to
	//                                      a solid fill of the first colour.
	//   loadMovie(url)                     gameswf_sprite_loadmovie - removes every child, resolves
	//                                      the image by name (render::create_bitmap_info_file), and
	//                                      places a `_____image` sprite at the next depth holding a
	//                                      `_____image_shape`: one tiled bitmap fill (type 0x40,
	//                                      matrix scale 0.05) over a w*20 x h*20 twips quad. This is
	//                                      the golden run's 320x240 background strip
	//                                      (setTexture id=24 320x240 clamp=0 m=[0.05 0 0|0 0.05 0]).
	// A path's pen position and style inheritance follow the shipped "new path" helper
	// (sub_10076A80): the new path copies fill0/fill1/line and starts at the last anchor.]
	// =============================================================================================
	static rgba	rgba_from_script(double rgb, double alpha_pct, bool has_alpha)
	{
		const int	c = (int) rgb;
		const Uint8	a = has_alpha ? (Uint8) (int) (alpha_pct * 2.55) : 255;
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

	// The shipped "new path" (sub_10076A80): inherit the current styles, start at the pen.
	static path*	canvas_new_path(shape_character_def* def)
	{
		path	np;
		const path*	cur = def->current_path();
		if (cur)
		{
			np.m_fill0 = cur->m_fill0;
			np.m_fill1 = cur->m_fill1;
			np.m_line = cur->m_line;
			canvas_pen(cur, &np.m_ax, &np.m_ay);
		}
		return def->add_path(np);
	}

	void	sprite_line_style(const fn_call& fn)
	{
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		int	idx = 0;
		if (fn.nargs > 0)
		{
			int	width = (int) (fn.arg(0).to_number() * 20.0);
			if (width == 0) width = 20;
			rgba	color(0, 0, 0, 255);
			if (fn.nargs > 1)
			{
				color = rgba_from_script(fn.arg(1).to_number(), fn.nargs > 2 ? fn.arg(2).to_number() : 100.0, fn.nargs > 2);
			}
			line_style	ls;
			ls.set((Uint16) width, color);
			idx = def->add_line_style(ls);
		}
		canvas_new_path(def)->m_line = idx;
	}

	void	sprite_move_to(const fn_call& fn)
	{
		if (fn.nargs != 2)
		{
			log_error("error: move_to needs two args\n");
			return;
		}
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		const float	x = (float) (fn.arg(0).to_number() * 20.0);
		const float	y = (float) (fn.arg(1).to_number() * 20.0);
		path*	cur = def->current_path();
		if (cur == NULL || cur->m_edges.size() > 0)
		{
			cur = canvas_new_path(def);
		}
		cur->m_ax = x;
		cur->m_ay = y;
	}

	void	sprite_line_to(const fn_call& fn)
	{
		if (fn.nargs != 2)
		{
			log_error("error: line_to needs two args\n");
			return;
		}
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		path*	cur = def->current_path();
		if (cur == NULL)
		{
			cur = canvas_new_path(def);
		}
		float	px, py;
		canvas_pen(cur, &px, &py);
		const float	x = (float) (fn.arg(0).to_number() * 20.0);
		const float	y = (float) (fn.arg(1).to_number() * 20.0);
		cur->m_edges.push_back(edge(px + (x - px) * 0.5f, py + (y - py) * 0.5f, x, y));
		def->refresh_bound();
	}

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
			if (px != cur->m_ax || py != cur->m_ay)
			{
				cur->m_edges.push_back(edge(px + (cur->m_ax - px) * 0.5f, py + (cur->m_ay - py) * 0.5f,
							    cur->m_ax, cur->m_ay));
			}
		}
		def->refresh_bound();
	}

	void	sprite_begin_fill(const fn_call& fn)
	{
		sprite_end_fill(fn);
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		int	idx = 0;
		if (fn.nargs > 0)
		{
			fill_style	fs;
			fs.set_solid(rgba_from_script(fn.arg(0).to_number(), fn.nargs > 1 ? fn.arg(1).to_number() : 100.0, fn.nargs > 1));
			idx = def->add_fill_style(fs);
		}
		// the shipped pair: a "new shape" marker path, then the path that carries the fill
		path	marker;
		marker.m_new_shape = true;
		def->add_path(marker);
		path	p;
		p.m_fill0 = idx;
		def->add_path(p);
	}

	void	sprite_begin_gradient_fill(const fn_call& fn)
	{
		log_error("beginGradientFill: gradients are not transcribed yet - filling with the first colour\n");
		sprite_end_fill(fn);
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;
		shape_character_def*	def = sprite->get_canvas_def();
		int	idx = 0;
		if (fn.nargs >= 2)
		{
			// arg1 is the colours array; take its first element
			as_object_interface*	colors = fn.arg(1).to_object();
			as_value	first;
			if (colors) colors->get_member("0", &first);
			fill_style	fs;
			fs.set_solid(rgba_from_script(first.to_number(), 100.0, false));
			idx = def->add_fill_style(fs);
		}
		path	marker;
		marker.m_new_shape = true;
		def->add_path(marker);
		path	p;
		p.m_fill0 = idx;
		def->add_path(p);
	}

	void	sprite_load_movie(const fn_call& fn)
	{
		if (fn.nargs < 1)
		{
			log_error("error: sprite_load_movie needs atleast one arg\n");
			return;
		}
		sprite_instance*	sprite = sprite_this(fn);
		if (sprite == NULL) return;

		// [TEMP] RW-89 target trace: which clip loads which image, where it sits. Remove with the tracers.
		{
			const matrix	wm = sprite->get_world_matrix();
			character*	par = sprite->get_parent() ? sprite->get_parent()->cast_to_character() : NULL;
			log_error("[loadMovie] target='%s' url='%s' world=[%g %g %g | %g %g %g] depth=%d parent='%s'\n",
				  sprite->get_name().c_str(), fn.arg(0).to_string(),
				  wm.m_[0][0], wm.m_[0][1], wm.m_[0][2], wm.m_[1][0], wm.m_[1][1], wm.m_[1][2],
				  sprite->get_depth(), par ? par->get_name().c_str() : "<none>");
		}

		sprite->remove_all_display_objects();

		const tu_string	url = fn.arg(0).to_tu_string();
		smart_ptr<bitmap_info>	bi = render::create_bitmap_info_file(url.c_str());
		if (bi == NULL)
		{
			return;
		}
		const int	w = bi->m_original_width;
		const int	h = bi->m_original_height;

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
		smart_ptr<bitmap_character>	bc = new bitmap_character(bi.get_ptr());
		smart_ptr<shape_character_def>	sh = new shape_character_def;
		fill_style	fs;
		matrix	bm;
		bm.set_identity();
		bm.m_[0][0] = 0.05f;
		bm.m_[1][1] = 0.05f;
		fs.set_bitmap(bc.get_ptr(), bm, false);
		const int	idx = sh->add_fill_style(fs);

		path	marker;
		marker.m_new_shape = true;
		sh->add_path(marker);
		path	p(0.0f, 0.0f, idx, 0, 0);
		const float	w20 = (float) w * 20.0f;
		const float	h20 = (float) h * 20.0f;
		p.m_edges.push_back(edge(w20 * 0.5f, 0.0f,        w20, 0.0f));
		p.m_edges.push_back(edge(w20,        h20 * 0.5f,  w20, h20));
		p.m_edges.push_back(edge(w20 * 0.5f, h20,         0.0f, h20));
		p.m_edges.push_back(edge(0.0f,       h20 * 0.5f,  0.0f, 0.0f));
		sh->add_path(p);
		sh->refresh_bound();

		// OWNERSHIP: generic_character::m_def is a RAW pointer in this gameswf (instances never own
		// their definitions; the movie's character table does). A runtime-built definition therefore
		// needs an owner that outlives the child, or the child dangles the moment this local dies -
		// retail run I/J died exactly so, in the next frame's mouse hit test, calling slot 5 of a
		// destroyed def (the bare ref_counted vtable + 5 = fontlib's accept_trapezoid). The image
		// sprite's canvas slot is that owner, the same one the drawing API's `_____defaultShape` uses;
		// remove_all_display_objects() releases both together. [CHOSEN, not recovered: DICE's owner
		// for the image shape is not attested; only that the child is named `_____image_shape`.]
		img_sprite->m_canvas = sh.get_ptr();
		img_sprite->add_display_object_def(sh.get_ptr(), "_____image_shape", 0);
	}

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

	void	sprite_goto_and_play(const fn_call& fn)
	{
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

		// [DICE-MOD SwiffPlayer: label or number, as gameswf_sprite_goto_and_play - see gotoAndStop.]
		if (fn.arg(0).get_type() == as_value::STRING)
		{
			sprite->goto_labeled_frame(fn.arg(0).to_string());
		}
		else
		{
			int	target_frame = int(fn.arg(0).to_number() - 1);	// Convert to 0-based
			sprite->goto_frame(target_frame);
		}
		sprite->set_play_state(movie_interface::PLAY);
	}

	void	sprite_goto_and_stop(const fn_call& fn)
	{
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

		// [DICE-MOD SwiffPlayer: a frame LABEL is accepted, as the shipped gameswf_sprite_goto_and_stop
		// does (STRING -> goto_labeled_frame, NUMBER -> goto_frame(n-1), then the play state either
		// way). Stock 2006 converted the argument to a number, so the menu's gotoAndStop(label) from
		// getFrameLabel() went to frame -1 and the root never left frame 0 (retail 2026-09-02).]
		if (fn.arg(0).get_type() == as_value::STRING)
		{
			sprite->goto_labeled_frame(fn.arg(0).to_string());
		}
		else
		{
			int	target_frame = int(fn.arg(0).to_number() - 1);	// Convert to 0-based
			sprite->goto_frame(target_frame);
		}
		sprite->set_play_state(movie_interface::STOP);
	}

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
			sprite->goto_frame(current_frame + 1);
		}
		sprite->set_play_state(movie_interface::STOP);
	}

	void	sprite_prev_frame(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);

		int current_frame = sprite->get_current_frame();
		if (current_frame > 0)
		{
			sprite->goto_frame(current_frame - 1);
		}
		sprite->set_play_state(movie_interface::STOP);
	}

	void	sprite_get_bytes_loaded(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);

		fn.result->set_int(sprite->get_root()->get_file_bytes());
	}

	void	sprite_get_bytes_total(const fn_call& fn)
	{
		sprite_instance* sprite = (sprite_instance*) fn.this_ptr;
		if (sprite == NULL)
		{
			sprite = (sprite_instance*) fn.env->get_target();
		}
		assert(sprite);

		fn.result->set_int(sprite->get_root()->get_file_bytes());
	}


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
		s_sprite_builtins->set_member("prevFrame", &sprite_prev_frame);
		s_sprite_builtins->set_member("getBytesLoaded", &sprite_get_bytes_loaded);
		s_sprite_builtins->set_member("getBytesTotal", &sprite_get_bytes_loaded);
		// [DICE-MOD SwiffPlayer: the shipped builtins the menu framework needs - see the bodies above.]
		s_sprite_builtins->set_member("createEmptyMovieClip", &sprite_create_empty_movieclip);
		s_sprite_builtins->set_member("attachMovie", &sprite_attach_movie);
		s_sprite_builtins->set_member("getNextHighestDepth", &sprite_get_next_highest_depth);
		s_sprite_builtins->set_member("removeMovieClip", &sprite_remove_movieclip);
		s_sprite_builtins->set_member("setMask", &sprite_set_mask);
		s_sprite_builtins->set_member("lineStyle", &sprite_line_style);
		s_sprite_builtins->set_member("moveTo", &sprite_move_to);
		s_sprite_builtins->set_member("lineTo", &sprite_line_to);
		s_sprite_builtins->set_member("beginFill", &sprite_begin_fill);
		s_sprite_builtins->set_member("endFill", &sprite_end_fill);
		s_sprite_builtins->set_member("beginGradientFill", &sprite_begin_gradient_fill);
		s_sprite_builtins->set_member("loadMovie", &sprite_load_movie);

		// @TODO
//		s_sprite_builtins->set_member("startDrag", &sprite_start_drag);
//		s_sprite_builtins->set_member("stopDrag", &sprite_stop_drag);
//		s_sprite_builtins->set_member("getURL", &sprite_get_url);
//		s_sprite_builtins->set_member("swapDepths", &sprite_swap_depths);
	}

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
		int	m_depth, m_id;

		remove_object_2() : m_depth(-1), m_id(-1) {}

		void	read(stream* in, int tag_type)
		{
			assert(tag_type == 5 || tag_type == 28);

			if (tag_type == 5)
			{
				// Older SWF's allow multiple objects at the same depth;
				// this m_id disambiguates.  Later SWF's just use one
				// object per depth.
				m_id = in->read_u16();
			}
			m_depth = in->read_u16();
		}

		virtual void	execute(movie* m)
		{
			m->remove_display_object(m_depth, m_id);
		}

		virtual void	execute_state(movie* m)
		{
			execute(m);
		}

		virtual void	execute_state_reverse(movie* m, int frame)
		{
			// reverse of remove is to re-add the previous object.
			execute_tag*	last_add = m->find_previous_replace_or_add_tag(frame, m_depth, m_id);
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

		virtual bool	is_remove_tag() const { return true; }

		// [DICE-MOD SwiffPlayer #45] the goto pass does nothing with a remove (0x1006F8E0 is a
		// nullsub); the state builder reads its depth.
		virtual void	execute_goto(movie* /*m*/) {}
		virtual int	get_tag_depth() const { return m_depth; }
	};


	void	remove_object_2_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 5 || tag_type == 28);

		remove_object_2*	t = new remove_object_2;
		t->read(in, tag_type);

		IF_VERBOSE_PARSE(log_msg("  remove_object_2(%d)\n", t->m_depth));

		m->add_execute_tag(t);
	}


	void	button_sound_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 17);

		int	button_character_id = in->read_u16();
		button_character_definition* ch = (button_character_definition*) m->get_character_def(button_character_id);
		assert(ch != NULL);

		ch->read(in, tag_type, m);
	}


	void	button_character_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 7 || tag_type == 34);

		int	character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  button character loader: char_id = %d\n", character_id));

		button_character_definition*	ch = new button_character_definition;
		ch->read(in, tag_type, m);

		m->add_character(character_id, ch);
	}


	//
	// export
	//


	void	export_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load an export tag (for exposing internal resources of m)
	{
		assert(tag_type == 56);

		int	count = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  export: count = %d\n", count));

		// Read the exports.
		for (int i = 0; i < count; i++)
		{
			Uint16	id = in->read_u16();
			char*	symbol_name = in->read_string();
			IF_VERBOSE_PARSE(log_msg("  export: id = %d, name = %s\n", id, symbol_name));

			if (font* f = m->get_font(id))
			{
				// Expose this font for export.
				m->export_resource(tu_string(symbol_name), f);
			}
			else if (character_def* ch = m->get_character_def(id))
			{
				// Expose this movie/button/whatever for export.
				m->export_resource(tu_string(symbol_name), ch);
			}
			else if (sound_sample* ch = m->get_sound_sample(id))
			{
				m->export_resource(tu_string(symbol_name), ch);
			}
			// [DICE-MOD SwiffPlayer: export BITMAP characters too.
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
			// See GAMESWF_DIVERGENCES.md.]
			else if (bitmap_character_def* bch = m->get_bitmap_character(id))
			{
				m->export_resource(tu_string(symbol_name), bch);
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


	void	import_loader(stream* in, int tag_type, movie_definition_sub* m)
	// Load an import tag (for pulling in external resources)
	{
		assert(tag_type == 57);

		char*	source_url = in->read_string();
		int	count = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  import: source_url = %s, count = %d\n", source_url, count));

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
			IF_VERBOSE_PARSE(log_msg("  import: id = %d, name = %s\n", id, symbol_name));

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
