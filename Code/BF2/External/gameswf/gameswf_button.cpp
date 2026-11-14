// gameswf_button.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// SWF buttons.  Mouse-sensitive update/display, actions, etc.


#include "gameswf_button.h"
#include "gameswf_log.h"

#include "gameswf_action.h"
#include "gameswf_render.h"
#include "gameswf_sound.h"
#include "gameswf_stream.h"


/*

Observations about button & mouse behavior

Entities that receive mouse events: only buttons and sprites, AFAIK

When the mouse button goes down, it becomes "captured" by whatever
element is topmost, directly below the mouse at that moment.  While
the mouse is captured, no other entity receives mouse events,
regardless of how the mouse or other elements move.

The mouse remains captured until the mouse button goes up.  The mouse
remains captured even if the element that captured it is removed from
the display list.

If the mouse isn't above a button or sprite when the mouse button goes
down, then the mouse is captured by the background (i.e. mouse events
just don't get sent, until the mouse button goes up again).

Mouse events:

+------------------+---------------+-------------------------------------+
| Event            | Mouse Button  | description                         |
=========================================================================
| onRollOver       |     up        | sent to topmost entity when mouse   |
|                  |               | cursor initially goes over it       |
+------------------+---------------+-------------------------------------+
| onRollOut        |     up        | when mouse leaves entity, after     |
|                  |               | onRollOver                          |
+------------------+---------------+-------------------------------------+
| onPress          |  up -> down   | sent to topmost entity when mouse   |
|                  |               | button goes down.  onRollOver       |
|                  |               | always precedes onPress.  Initiates |
|                  |               | mouse capture.                      |
+------------------+---------------+-------------------------------------+
| onRelease        |  down -> up   | sent to active entity if mouse goes |
|                  |               | up while over the element           |
+------------------+---------------+-------------------------------------+
| onDragOut        |     down      | sent to active entity if mouse      |
|                  |               | is no longer over the entity        |
+------------------+---------------+-------------------------------------+
| onReleaseOutside |  down -> up   | sent to active entity if mouse goes |
|                  |               | up while not over the entity.       |
|                  |               | onDragOut always precedes           |
|                  |               | onReleaseOutside                    |
+------------------+---------------+-------------------------------------+
| onDragOver       |     down      | sent to active entity if mouse is   |
|                  |               | dragged back over it after          |
|                  |               | onDragOut                           |
+------------------+---------------+-------------------------------------+

There is always one active entity at any given time (considering NULL to
be an active entity, representing the background, and other objects that
don't receive mouse events).

When the mouse button is up, the active entity is the topmost element
directly under the mouse pointer.

When the mouse button is down, the active entity remains whatever it
was when the button last went down.

The active entity is the only object that receives mouse events.

!!! The "trackAsMenu" property alters this behavior!  If trackAsMenu
is set on the active entity, then onReleaseOutside is filtered out,
and onDragOver from another entity is allowed (from the background, or
another trackAsMenu entity). !!!


Pseudocode:

active_entity = NULL
mouse_button_state = UP
mouse_inside_entity_state = false
frame loop:
  if mouse_button_state == DOWN

    // Handle trackAsMenu
    if (active_entity->trackAsMenu)
      possible_entity = topmost entity below mouse
      if (possible_entity != active_entity && possible_entity->trackAsMenu)
        // Transfer to possible entity
	active_entity = possible_entity
	active_entity->onDragOver()
	mouse_inside_entity_state = true;

    // Handle onDragOut, onDragOver
    if (mouse_inside_entity_state == false)
      if (mouse is actually inside the active_entity)
        // onDragOver
	active_entity->onDragOver()
        mouse_inside_entity_state = true;

    else // mouse_inside_entity_state == true
      if (mouse is actually outside the active_entity)
        // onDragOut
	active_entity->onDragOut()
	mouse_inside_entity_state = false;

    // Handle onRelease, onReleaseOutside
    if (mouse button is up)
      if (mouse_inside_entity_state)
        // onRelease
        active_entity->onRelease()
      else
        // onReleaseOutside
	if (active_entity->trackAsMenu == false)
          active_entity->onReleaseOutside()
      mouse_button_state = UP
    
  if mouse_button_state == UP
    new_active_entity = topmost entity below the mouse
    if (new_active_entity != active_entity)
      // onRollOut, onRollOver
      active_entity->onRollOut()
      active_entity = new_active_entity
      active_entity->onRollOver()
    
    // Handle press
    if (mouse button is down)
      // onPress
      active_entity->onPress()
      mouse_inside_entity_state = true
      mouse_button_state = DOWN

*/


namespace gameswf
{
#if !defined(_MSC_VER)
	const char*	swf_list_trace_path(character* ch);	// [TEMP] gameswf_impl.cpp
#endif
	//  generate_mouse_button_events (and the RW-89 tracer that lived in it)
	// are gone: the shipped player has no root-level dispatcher, every character runs its own mouse
	// pass - for buttons, button_character_instance::mouse_update below.

	struct button_character_instance : public character
	{
		button_character_definition*	m_def;
		array< smart_ptr<character> >	m_record_character;

		enum mouse_flags
		{
			IDLE = 0,
			FLAG_OVER = 1,
			FLAG_DOWN = 2,
			OVER_DOWN = FLAG_OVER|FLAG_DOWN,

			// aliases
			OVER_UP = FLAG_OVER,
			OUT_DOWN = FLAG_DOWN
		};
		int	m_last_mouse_flags, m_mouse_flags;
		enum e_mouse_state
		{
			UP = 0,
			DOWN,
			OVER
		};
		e_mouse_state m_mouse_state;
		//  the shipped button's +216 first-frame latch and +217 held press;
		// m_last_mouse_flags / m_mouse_flags above are its +220 / +224 (bit 0 inside, bit 1 down).
		bool	m_first_mouse_frame;
		bool	m_mouse_pressed;

		// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. The state
		// characters are walked too; the button keeps owning them.
		virtual void	release_script_values(array<as_value>* held)
		{
			character::release_script_values(held);
			for (int i = 0; i < m_record_character.size(); i++)
			{
				if (m_record_character[i] != NULL)
				{
					held->push_back(as_value(m_record_character[i].get_ptr()));
				}
			}
		}

		// SwiffPlayer.dll 0x100A2C50; native layout and tree-root ownership are retained.
		button_character_instance(button_character_definition* def, movie* parent, int id)
			:
			character(parent, id),
			m_def(def),
			m_last_mouse_flags(IDLE),
			m_mouse_flags(IDLE),
			m_mouse_state(UP),
			m_first_mouse_frame(true),
			m_mouse_pressed(false)
		{
			assert(m_def);

			int r, r_num =  m_def->m_button_records.size();
			m_record_character.resize(r_num);

			movie_definition_sub*	movie_def = static_cast<movie_definition_sub*>(
				parent->get_root_movie()->get_movie_definition());

			for (r = 0; r < r_num; r++)
			{
				button_record*	bdef = &m_def->m_button_records[r];

				if (bdef->m_character_def == NULL)
				{
					// Resolve the character id.
					bdef->m_character_def = movie_def->get_character_def(bdef->m_character_id);
				}
				assert(bdef->m_character_def != NULL);

				const matrix&	mat = m_def->m_button_records[r].m_button_matrix;
				const cxform&	cx = m_def->m_button_records[r].m_button_cxform;

				smart_ptr<character>	ch = bdef->m_character_def->create_character_instance(this, id);
				m_record_character[r] = ch;
				ch->set_matrix(mat);
				ch->set_cxform(cx);
				ch->restart();
			}
		}

		// Compiler deleting wrapper: SwiffPlayer.dll 0x100A3BF0; the lifetime
		// body at 0x100A3C30 remains separate from forwarding and conditional deletion.
		~button_character_instance()
		{
			// SwiffPlayer.dll 0x100A3C30 releases this button's capture before
			// record characters. Re-read the publication after the capture query.
			if (get_global_root_movie() != NULL
				&& get_global_root_movie()->get_active_mouse_entity() == this)
			{
				get_global_root_movie()->set_active_mouse_entity(NULL);
			}
		}

		// SwiffPlayer.dll 0x100A2E20 delegates to the immediate parent's root.
		movie_root*	get_root() { return get_parent()->get_root(); }
		movie*	get_root_movie() { return get_parent()->get_root_movie(); }

		// SwiffPlayer.dll 0x100A2E30 forwards to the definition bounds slot.
		rect get_bound_copy() const override { return m_def->get_bound_copy(); }

		// SwiffPlayer.dll 0x100A2EA0 leaves the first-frame and held-press latches alone.
		void	restart()
		{
			m_last_mouse_flags = IDLE;
			m_mouse_flags = IDLE;
			m_mouse_state = UP;
			int r, r_num =  m_record_character.size();
			for (r = 0; r < r_num; r++)
			{
				m_record_character[r]->restart();
			}
		}


		virtual void	advance(float delta_time)
		{
			// Implement mouse-drag.
			character::do_mouse_drag();


			// Advance our relevant characters.
			{for (int i = 0; i < m_def->m_button_records.size(); i++)
			{
				button_record&	rec = m_def->m_button_records[i];
				if (m_record_character[i] == NULL)
				{
					continue;
				}

				// Matrix
				// SwiffPlayer.dll 0x100A2EE0
				// fetches the world matrix once per non-null record, before testing state.
				matrix mat = get_world_matrix();
				matrix sub_matrix = mat;
				sub_matrix.concatenate(rec.m_button_matrix);

				// Advance characters that are activated by the new mouse state
				if (((m_mouse_state == UP) && (rec.m_up)) ||
				    ((m_mouse_state == DOWN) && (rec.m_down)) ||
				    ((m_mouse_state == OVER) && (rec.m_over)))
				{
					m_record_character[i]->advance(delta_time);
				}
			}}
		}


		// SwiffPlayer.dll 0x100A2FB0.
		void	display()
		{
			for (int i = 0; i < m_def->m_button_records.size(); i++)
			{
				button_record&	rec = m_def->m_button_records[i];
				if (m_record_character[i] == NULL)
				{
					continue;
				}
				if ((m_mouse_state == UP && rec.m_up)
				    || (m_mouse_state == DOWN && rec.m_down)
				    || (m_mouse_state == OVER && rec.m_over))
				{
					m_record_character[i]->display();
				}
			}

		}

		inline int	transition(int a, int b) const
		// Combine the flags to avoid a conditional. It would be faster with a macro.
		{
			return (a << 2) | b;
		}


		//  sub_100A3020 (1109 bytes, read in full 2026-09-13) - the button's
		// mouse pass, vtable slot 4: stock's generate_mouse_button_events + on_button_event fused into
		// the same shape as the sprite's. A per-frame (inside, down) state against the previous frame,
		// run only while nobody else holds the pointer; inside means one of the definition's hit-test
		// records covers the pointer in that record's own space, and nothing is tested once a sibling
		// above consumed the point. The transitions become the definition's button_action condition
		// bits directly (the sound index and the record restart key on them too), the trackAsMenu
		// definitions taking the menu set. A button never covers the point for its parent's
		// point_test_world (the shipped slot 61 is the default false), and one whose definition has no
		// actions never keeps the pointer captured.
		virtual bool	mouse_update(bool consumed)
		{
			// [CHOSEN, not recovered] S2 bridge: tree ownership replaces the retail
			// global root; 0x100A3020 uses the immediate parent for capture/input.
			movie*	root = get_parent();
			if (root == NULL)
			{
				return false;
			}
			movie*	active = root->get_active_mouse_entity();
			if (active != NULL && active != this)
			{
				return false;
			}
			matrix	world = get_world_matrix();
			int	mx = 0, my = 0, mb = 0;
			root->get_mouse_state(&mx, &my, &mb);
			m_last_mouse_flags = m_mouse_flags;
			m_mouse_flags = 0;
			if (!consumed)
			{
				point	p;
				world.transform_by_inverse(&p, point(PIXELS_TO_TWIPS((float) mx), PIXELS_TO_TWIPS((float) my)));
				for (int i = 0; i < m_def->m_button_records.size(); i++)
				{
					button_record&	rec = m_def->m_button_records[i];
					if (rec.m_character_id < 0 || rec.m_hit_test == false)
					{
						continue;
					}
					point	sub_p;
					rec.m_button_matrix.transform_by_inverse(&sub_p, p);
					if (rec.m_character_def->point_test_local(sub_p.m_x, sub_p.m_y))
					{
						m_mouse_flags |= 1;
						break;
					}
				}
				if (mb)
				{
					m_mouse_flags |= 2;
				}
			}
			if (m_first_mouse_frame)
			{
				m_last_mouse_flags = m_mouse_flags;
				m_first_mouse_frame = false;
			}
			if (m_mouse_flags != m_last_mouse_flags)
			{
				const int	t = m_mouse_flags | (m_last_mouse_flags << 2);
				int	c = 0;
				if (t == 1)
				{
					c = button_action::IDLE_TO_OVER_UP;
					m_mouse_state = OVER;
				}
				else if (t == 4)
				{
					c = button_action::OVER_UP_TO_IDLE;
					m_mouse_state = UP;
				}
				else if (m_def->m_menu)
				{
					switch (t)
					{
					case 7:
						c = button_action::OVER_UP_TO_OVER_DOWN;
						m_mouse_state = DOWN;
						break;
					case 13:
					case 9:
						if (m_mouse_pressed)
						{
							c = button_action::OVER_DOWN_TO_OVER_UP;
							m_mouse_state = OVER;
						}
						break;
					case 3:
						c = button_action::IDLE_TO_OVER_DOWN;
						m_mouse_state = DOWN;
						break;
					case 12:
						c = button_action::OVER_DOWN_TO_IDLE;
						m_mouse_state = UP;
						break;
					default:
						break;
					}
				}
				else
				{
					switch (t)
					{
					case 7:
						m_mouse_pressed = true;
						c = button_action::OVER_UP_TO_OVER_DOWN;
						m_mouse_state = DOWN;
						root->set_active_mouse_entity(this);
						break;
					case 13:
						m_mouse_state = OVER;
						if (!m_mouse_pressed)
						{
							c = button_action::IDLE_TO_OVER_UP;
						}
						else
						{
							m_mouse_pressed = false;
							c = button_action::OVER_DOWN_TO_OVER_UP;
							root->set_active_mouse_entity(NULL);
						}
						break;
					case 11:
						c = button_action::OUT_DOWN_TO_OVER_DOWN;
						m_mouse_state = DOWN;
						break;
					case 14:
						c = button_action::OVER_DOWN_TO_OUT_DOWN;
						m_mouse_state = UP;
						break;
					default:
						if (m_mouse_pressed && (t == 8 || t == 12))
						{
							// Released away from the button: still a release when this button is the
							// one being dragged (the root's drag state), a release-outside otherwise.
							movie::drag_state	st;
							root->get_drag_state(&st);
							m_mouse_pressed = false;
							if (st.m_character == this)
							{
								m_mouse_state = OVER;
								c = button_action::OVER_DOWN_TO_OVER_UP;
							}
							else
							{
								c = button_action::OUT_DOWN_TO_IDLE;
								m_mouse_state = UP;
							}
							root->set_active_mouse_entity(NULL);
						}
						break;
					}
				}
				fire_transition(c);
			}
			return (m_mouse_flags & 1) != 0;
		}




		//  the tail of sub_100A3020 for one transition (a button_action
		// condition bit): the transition sound - the definition's sound records are indexed 0 roll
		// out, 1 roll over, 2 press, 3 release - the capture dropped when the definition has no
		// actions at all, the record restart, then the definition's actions whose conditions include
		// the bit. SwiffPlayer.dll 0x100A3410..0x100A3436 executes actions immediately
		// in the parent's environment.
		void	fire_transition(int c)
		{
#if !defined(_MSC_VER)
			// [TEMP] c-swf-disconnect: a button transition.
			if (swf_list_trace_on())
			{
				SWF_LIST_TRACE("button cond=0x%x on '%s' (%d action records)", c, swf_list_trace_path(this),
					m_def->m_button_actions.size());
			}
#endif
			if (m_def->m_sound != NULL)
			{
				sound_handler* s = get_sound_handler();
				if (s != NULL)
				{
					int bi = -1;
					switch (c)
					{
					case button_action::OVER_UP_TO_IDLE:      bi = 0; break;
					case button_action::IDLE_TO_OVER_UP:      bi = 1; break;
					case button_action::OVER_UP_TO_OVER_DOWN: bi = 2; break;
					case button_action::OVER_DOWN_TO_OVER_UP: bi = 3; break;
					default: break;
					}
					if (bi >= 0)
					{
						button_character_definition::button_sound_info& bs = m_def->m_sound->m_button_sounds[bi];
						// character zero is considered as null character
						if (bs.m_sound_id > 0)
						{
							assert(m_def->m_sound->m_button_sounds[bi].m_sam != NULL);
							if (bs.m_sound_style.m_stop_playback)
							{
								s->stop_sound(bs.m_sam->m_sound_handler_id);
							}
							else
							{
								s->play_sound(bs.m_sam->m_sound_handler_id, bs.m_sound_style.m_loop_count);
							}
						}
					}
				}
			}
			if (m_def->m_button_actions.size() == 0)
			{
				get_root_movie()->set_active_mouse_entity(NULL);
			}

			// restart the characters of the new state.
			restart_characters(c);

			// Execute the matching transition actions.
			{for (int i = 0; i < m_def->m_button_actions.size(); i++)
			{
				if (m_def->m_button_actions[i].m_conditions & c)
				{
					// Matching action.
					for (int j = 0; j < m_def->m_button_actions[i].m_actions.size(); j++)
					{
						// [CHOSEN, not recovered] S2 bridge to retail's
						// parent secondary +0x2C -> action_buffer::execute (0x100961E0).
						m_def->m_button_actions[i].m_actions[j]->execute(get_parent()->get_action_environment());
					}
				}
			}}
		}


		void restart_characters(int condition)
		{
			// SwiffPlayer.dll 0x100A34A0:
			// restart every non-null record, including transitions with condition zero.
			for (int i = 0; i < m_def->m_button_records.size(); i++)
			{
				if (m_record_character[i] != NULL)
				{
					m_record_character[i]->restart();
				}
			}
		}


		virtual void	get_mouse_state(int* x, int* y, int* buttons)
		{
			get_parent()->get_mouse_state(x, y, buttons);
		}

		// [CHOSEN, not recovered] S2 bridge: tree root replaces the retail global
		// accessed by 0x100A2E70/0x100A2E80. It owns the same shared capture state.
		virtual movie* get_active_mouse_entity() { return get_root_movie()->get_active_mouse_entity(); }
		virtual void set_active_mouse_entity(movie* m) { get_root_movie()->set_active_mouse_entity(m); }


		//
		// ActionScript overrides
		//

		virtual void	set_member(const tu_stringi& name, const as_value& val)
		{
			// SwiffPlayer.dll 0x100A34E0.
			// Retail compares these names case-sensitively and has no width/height arm.
			const char* key = name.c_str();
			if (strcmp(key, "_x") == 0 || strcmp(key, "_y") == 0)
			{
				matrix m = get_matrix();
				m.m_[strcmp(key, "_x") == 0 ? 0 : 1][2] = float(val.to_number() * 20.0);
				set_matrix(m);
				return;
			}
			if (strcmp(key, "_xscale") == 0)
			{
				matrix m = get_matrix();
				const float scale = float(val.to_number() * 0.01f);
				const float y_scale = m.get_y_scale();
				const float rotation = m.get_rotation();
				m.set_scale_rotation(scale, y_scale, rotation);
				set_matrix(m);
				return;
			}
			if (strcmp(key, "_yscale") == 0)
			{
				matrix m = get_matrix();
				const float x_scale = m.get_x_scale();
				const float scale = float(val.to_number() * 0.01f);
				const float rotation = m.get_rotation();
				m.set_scale_rotation(x_scale, scale, rotation);
				set_matrix(m);
				return;
			}
			if (strcmp(key, "_alpha") == 0)
			{
				cxform cx = get_cxform();
				cx.m_[3][0] = float(val.to_number() * 0.01f);
				set_cxform(cx);
				return;
			}
			if (strcmp(key, "_visible") == 0)
			{
				m_visible = val.to_bool();
				return;
			}

			log_error("error: button_character_instance::set_member('%s', '%s') not implemented yet\n",
					  name.c_str(),
					  val.to_string());
		}

		virtual bool	get_member(const tu_stringi& name, as_value* val)
		{
			// SwiffPlayer.dll 0x100A37D0.
			const char* key = name.c_str();
			if (strcmp(key, "_x") == 0 || strcmp(key, "_y") == 0)
			{
				matrix m = get_matrix();
				val->set_double(double(m.m_[strcmp(key, "_x") == 0 ? 0 : 1][2]) * double(0.05f));
				return true;
			}
			if (strcmp(key, "_xscale") == 0 || strcmp(key, "_yscale") == 0)
			{
				matrix m = get_matrix();
				const float scale = strcmp(key, "_xscale") == 0 ? m.get_x_scale() : m.get_y_scale();
				val->set_double(double(scale) * 100.0);
				return true;
			}
			if (strcmp(key, "_alpha") == 0)
			{
				val->set_double(double(get_cxform().m_[3][0]) * 100.0);
				return true;
			}
			if (strcmp(key, "_visible") == 0)
			{
				val->set_bool(m_visible);
				return true;
			}
			log_error("error: button_character_instance::get_member('%s') not implemented yet\n", key);

			return false;
		}

		// not sure if we need to override this one.
		//virtual const char*	get_text_value() const { return NULL; }	// edit_text_character overrides this
	};


	//
	// button_record
	//

	// SwiffPlayer.dll 0x100A1BE0.
	bool	button_record::read(stream* in, int tag_type, movie_definition_sub* m)
	// Return true if we read a record; false if this is a null record.
	{
		int	flags = in->read_u8();
		if (flags == 0)
		{
			return false;
		}
		m_hit_test = flags & 8 ? true : false;
		m_down = flags & 4 ? true : false;
		m_over = flags & 2 ? true : false;
		m_up = flags & 1 ? true : false;

		m_character_id = in->read_u16();
		m_character_def = NULL;
		m_button_layer = in->read_u16(); 
		m_button_matrix.read(in);

		if (tag_type == 34)
		{
			m_button_cxform.read_rgba(in);
		}

		return true;
	}


	//
	// button_action
	//


	// SwiffPlayer.dll 0x100A2710.
	button_action::~button_action()
	{
		for (int i = 0, n = m_actions.size(); i < n; i++)
		{
			delete m_actions[i];
		}
		m_actions.resize(0);
	}

	// SwiffPlayer.dll 0x100A2580.
	void	button_action::read(stream* in, int tag_type)
	{
		// Read condition flags.
		if (tag_type == 7)
		{
			m_conditions = OVER_DOWN_TO_OVER_UP;
		}
		else
		{
			assert(tag_type == 34);
			m_conditions = in->read_u16();
		}

		// Read actions.
		IF_VERBOSE_ACTION(log_msg("-- actions in button\n")); // @@ need more info about which actions
		action_buffer*	a = new action_buffer;
		a->read(in);
		m_actions.push_back(a);
	}


	//
	// button_character_definition
	//

	// SwiffPlayer.dll 0x100A3A60; the tree assigns character IDs separately.
	button_character_definition::button_character_definition()
		:
		m_sound(NULL)
	// Constructor.
	{
	}

	// SwiffPlayer.dll 0x100A3AF0; compiler deleting wrapper 0x100A3CB0.
	button_character_definition::~button_character_definition()
	{
		delete m_sound;
	}

	// SwiffPlayer.dll 0x100A2010.
	// Keep equal-layer insertion and the shipped halving search order.
	int button_character_definition::find_record_index(int layer) const
	{
		const int count = m_button_records.size();
		if (count == 0) return 0;
		int index = count >> 1;
		int step = index;
		for (;;)
		{
			step >>= 1;
			if (step < 1) step = 1;
			const int current = m_button_records[index].m_button_layer;
			if (layer > current)
			{
				if (index == count - 1) return count;
				index += step;
			}
			else
			{
				if (layer == current || index == 0 || m_button_records[index - 1].m_button_layer < layer)
				{
					return index;
				}
				index -= step;
			}
		}
	}


	// SwiffPlayer.dll 0x100A22D0.
	void button_character_definition::sound_info::read(stream* in)
	{
		m_in_point = m_out_point = m_loop_count = 0;
		in->read_uint(2);	// skip reserved bits.
		m_stop_playback = in->read_uint(1) ? true : false;
		m_no_multiple = in->read_uint(1) ? true : false;
		m_has_envelope = in->read_uint(1) ? true : false;
		m_has_loops = in->read_uint(1) ? true : false;
		m_has_out_point = in->read_uint(1) ? true : false;
		m_has_in_point = in->read_uint(1) ? true : false;
		if (m_has_in_point) m_in_point = in->read_u32();
		if (m_has_out_point) m_out_point = in->read_u32();
		if (m_has_loops) m_loop_count = in->read_u16();
		if (m_has_envelope)
		{
			int nPoints = in->read_u8();
			m_envelopes.resize(nPoints);
			for (int i=0; i < nPoints; i++)
			{
				m_envelopes[i].m_mark44 = in->read_u32();
				m_envelopes[i].m_level0 = in->read_u16();
				m_envelopes[i].m_level1 = in->read_u16();
			}
		}
		else
		{
			m_envelopes.resize(0);
		}
		IF_VERBOSE_PARSE(
			log_msg("	has_envelope = %d\n", m_has_envelope);
			log_msg("	has_loops = %d\n", m_has_loops);
			log_msg("	has_out_point = %d\n", m_has_out_point);
			log_msg("	has_in_point = %d\n", m_has_in_point);
			log_msg("	in_point = %d\n", m_in_point);
			log_msg("	out_point = %d\n", m_out_point);

			log_msg("	loop_count = %d\n", m_loop_count);
			log_msg("	envelope size = %d\n", m_envelopes.size());
		);
	}



	// SwiffPlayer.dll 0x100A1F90. Retail does not initialize this accumulator,
	// even for nonempty buttons; preserve that defect rather than inventing a seed.
	rect button_character_definition::get_bound_copy() const
	{
		rect bounds;
		for (int i = 0, n = m_button_records.size(); i < n; ++i)
		{
			character_def* definition = m_button_records[i].m_character_def;
			if (definition)
			{
				rect child = definition->get_bound_copy();                    // +0x20
				child.transform(m_button_records[i].m_button_matrix);
				bounds.expand_to_rect(child);
			}
		}
		return bounds;
	}

	// SwiffPlayer.dll 0x100A2960.
	void	button_character_definition::read(stream* in, int tag_type, movie_definition_sub* m)
	// Initialize from the given stream.
	{
		assert(tag_type == 7 || tag_type == 17 || tag_type == 34);

		if (tag_type == 7)
		{
			// Old button tag.
				
			// Read button character records.
			for (;;)
			{
				button_record	r;
				if (r.read(in, tag_type, m) == false)
				{
					// Null record; marks the end of button records.
					break;
				}
				// SwiffPlayer.dll 0x100A2960.
				m_button_records.insert(find_record_index(r.m_button_layer), r);
			}

			// Read actions.
			m_button_actions.resize(m_button_actions.size() + 1);
			m_button_actions.back().read(in, tag_type);
		}
		else if (tag_type == 17)
		{
			assert(m_sound == NULL);	// redefinition button sound is error
			m_sound = new button_sound_def();
			IF_VERBOSE_PARSE(log_msg("button sound options:\n"));
			for (int i = 0; i < 4; i++)
			{
				button_sound_info& bs = m_sound->m_button_sounds[i];
				bs.m_sound_id = in->read_u16();
				if (bs.m_sound_id > 0)
				{
					bs.m_sam = (sound_sample_impl*) m->get_sound_sample(bs.m_sound_id);
					if (bs.m_sam == NULL)
					{
//						printf("sound tag not found, sound_id=%d, button state #=%i", bs.sound_id, i);
					}
					IF_VERBOSE_PARSE(log_msg("\n	sound_id = %d\n", bs.m_sound_id));
					bs.m_sound_style.read(in);
				}
			}
		}
		else if (tag_type == 34)
		{
			// Read the menu flag.
			m_menu = in->read_u8() != 0;

			int	button_2_action_offset = in->read_u16();
			int	next_action_pos = in->get_position() + button_2_action_offset - 2;

			// Read button records.
			for (;;)
			{
				button_record	r;
				if (r.read(in, tag_type, m) == false)
				{
					// Null record; marks the end of button records.
					break;
				}
				// SwiffPlayer.dll 0x100A2960.
				m_button_records.insert(find_record_index(r.m_button_layer), r);
			}

			if (button_2_action_offset > 0)
			{
				in->set_position(next_action_pos);

				// Read Button2ActionConditions
				for (;;)
				{
					int	next_action_offset = in->read_u16();
					next_action_pos = in->get_position() + next_action_offset - 2;

					m_button_actions.resize(m_button_actions.size() + 1);
					m_button_actions.back().read(in, tag_type);

					if (next_action_offset == 0
					    || in->get_position() >= in->get_tag_end_position())
					{
						// done.
						break;
					}

					// seek to next action.
					in->set_position(next_action_pos);
				}
			}
		}
	}


	// SwiffPlayer.dll 0x100A3B70.
	character*	button_character_definition::create_character_instance(movie* parent, int id)
	// Create a mutable instance of our definition.
	{
		character*	ch = new button_character_instance(this, parent, id);
		return ch;
	}
};


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
