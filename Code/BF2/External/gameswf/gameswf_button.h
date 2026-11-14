// gameswf_button.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// SWF buttons.  Mouse-sensitive update/display, actions, etc.


#ifndef GAMESWF_BUTTON_H
#define GAMESWF_BUTTON_H


#include "gameswf_impl.h"
#include "gameswf_sound.h"


namespace gameswf
{
	//
	// Helper to generate mouse events, given mouse state & history.
	//

	struct mouse_button_state
	{
		weak_ptr<movie>	m_active_entity;	// entity that currently owns the mouse pointer
		weak_ptr<movie>	m_topmost_entity;	// what's underneath the mouse right now

		bool	m_mouse_button_state_last;		// previous state of mouse button
		bool	m_mouse_button_state_current;		// current state of mouse button

		bool	m_mouse_inside_entity_last;	// whether mouse was inside the active_entity last frame

		mouse_button_state()
			:
			m_mouse_button_state_last(0),
			m_mouse_button_state_current(0),
			m_mouse_inside_entity_last(false)
		{
		}
	};

	//  generate_mouse_button_events is gone: the shipped player has no
	// root-level dispatcher, every character runs its own mouse pass (movie::mouse_update). The
	// state record stays for its m_active_entity, the root's mouse capture.


	//
	// button characters
	//
	enum mouse_state
	{
		MOUSE_UP,
		MOUSE_DOWN,
		MOUSE_OVER
	};

	struct button_record
	{
		// Implicit construction: SwiffPlayer.dll 0x100A1C70 initializes the
		// matrix then cxform; flags, ID, definition and layer remain unwritten.
		bool	m_hit_test;
		bool	m_down;
		bool	m_over;
		bool	m_up;
		int	m_character_id;
		character_def*	m_character_def;
		int	m_button_layer;
		matrix	m_button_matrix;
		cxform	m_button_cxform;

		bool	read(stream* in, int tag_type, movie_definition_sub* m);
	};
	

	struct button_action
	{
		enum condition
		{
			IDLE_TO_OVER_UP = 1 << 0,
			OVER_UP_TO_IDLE = 1 << 1,
			OVER_UP_TO_OVER_DOWN = 1 << 2,
			OVER_DOWN_TO_OVER_UP = 1 << 3,
			OVER_DOWN_TO_OUT_DOWN = 1 << 4,
			OUT_DOWN_TO_OVER_DOWN = 1 << 5,
			OUT_DOWN_TO_IDLE = 1 << 6,
			IDLE_TO_OVER_DOWN = 1 << 7,
			OVER_DOWN_TO_IDLE = 1 << 8,
		};
		int	m_conditions;
		// SwiffPlayer.dll 0x100A2560: implicit construction initializes only the array.
		array<action_buffer*>	m_actions;

		// SwiffPlayer.dll 0x100A2840: deleting wrapper invokes this destructor
		// before the conditional object free.
		~button_action();
		void	read(stream* in, int tag_type);
	};


	struct button_character_definition : public character_def
	{
		struct sound_envelope
		{
			Uint32 m_mark44;
			Uint16 m_level0;
			Uint16 m_level1;
		};

		struct sound_info
		{
			// SwiffPlayer.dll 0x100A2610 / 0x100A2600: implicit construction
			// and destruction affect only the envelope array; scalars stay uninitialized.
			void read(stream* in);

			bool m_no_multiple;
			bool m_stop_playback;
			bool m_has_envelope;
			bool m_has_loops;
			bool m_has_out_point;
			bool m_has_in_point;
			Uint32 m_in_point;
			Uint32 m_out_point;
			Uint16 m_loop_count;
			array<sound_envelope> m_envelopes;
		};

		struct button_sound_info
		{
			// SwiffPlayer.dll 0x100A2640 / 0x100A2630: sound_info supplies
			// the only member lifetime work; the ID and sample pointer stay uninitialized.
			Uint16 m_sound_id;
			sound_sample_impl*	m_sam;
			sound_info m_sound_style;
		};

		struct button_sound_def
		{
			void	read(stream* in, movie_definition_sub* m);
			// SwiffPlayer.dll 0x100A26E0 / 0x100A2680: construct four envelope
			// arrays, then destroy them in reverse order; scalar fields stay uninitialized.
			// The deleting wrapper at 0x100A2810 destroys them before freeing the object.
			button_sound_info m_button_sounds[4];
		};


		bool m_menu;
		array<button_record>	m_button_records;
		array<button_action>	m_button_actions;
		button_sound_def*	m_sound;

		button_character_definition();
		virtual ~button_character_definition();
		// Retail leaves the accumulator unwritten before including its records.
		// The returned rectangle is indeterminate for an empty button as well.
		rect get_bound_copy() const override;

		character*	create_character_instance(movie* parent, int id);
		void	read(stream* in, int tag_type, movie_definition_sub* m);
		// SwiffPlayer.dll 0x100A2010: find the insertion point in layer order.
		int	find_record_index(int layer) const;
	};

};	// end namespace gameswf


#endif // GAMESWF_BUTTON_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
