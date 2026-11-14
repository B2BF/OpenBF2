// gameswf_action.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Implementation and helpers for SWF actions.


#ifndef GAMESWF_ACTION_H
#define GAMESWF_ACTION_H


#include <wchar.h>

#include "gameswf.h"
#include "gameswf_types.h"
#include "base/container.h"
#include "base/smart_ptr.h"


namespace gameswf
{
	struct movie;
	struct as_environment;
	struct as_object_interface;
	struct as_value;
	struct as_as_function;
	void	log_error(const char* fmt, ...);	// [TEMP] RW-89 trace support (gameswf_log.h)
	struct as_object;
	//  a fresh Array and its push, for String.split (the array type lives in
	// gameswf_action.cpp; the String unit only needs these two).
	as_object*	create_array_object();
	//  DICE's String object from wide text (gameswf_string.cpp).
	as_object_interface*	create_dice_string_object(const Uint16* wide, int count);
	//  Assign the wide payload of an existing factory object.
	void	assign_dice_string_object_wide(as_object_interface* object, const Uint16* wide);
	//  the narrow twin: Mod.getCurrentModName (sub_1004F950) assigns the
	// +44 tu_string from a plain host string and CLEARS the +60 wide side, the mirror of the above.
	void	assign_dice_string_object_narrow(as_object_interface* object, const char* text);

#if !defined(_WIN32)
	// [CHOSEN, not recovered] gameswf and DICE's String object keep text as 16-bit Windows
	// wchar_t units (Uint16). The host's std::wstring/wchar_t is 32 bits on POSIX, so the
	// shipped `reinterpret_cast<const Uint16*>(wide)` handed over 'W',0,'e',0,... and every
	// localized menu string ended after its first character. dice_utf16 converts one wchar_t
	// to one Uint16 (count-preserving; above U+FFFF becomes U+FFFD). The temporary lives to
	// the end of the full expression that names it.
	struct dice_utf16
	{
		explicit dice_utf16(const wchar_t* wide)
		{
			if (!wide) { m_null = true; return; }
			for (; *wide; ++wide) m_units.push_back(unit(*wide));
			m_units.push_back(0);
		}
		operator const Uint16*() const { return m_null ? 0 : &m_units[0]; }
	private:
		static Uint16	unit(wchar_t c) { return (unsigned int) c > 0xFFFFu ? (Uint16) 0xFFFD : (Uint16) c; }
		array<Uint16>	m_units;
		bool	m_null = false;
	};
	// The opposite boundary: Uint16 text to the renderer's native wchar_t (text_set).
	struct dice_wide
	{
		explicit dice_wide(const Uint16* units)
		{
			if (units) for (; *units; ++units) m_chars.push_back((wchar_t) *units);
			m_chars.push_back(0);
		}
		operator const wchar_t*() const { return &m_chars[0]; }
	private:
		array<wchar_t>	m_chars;
	};
#define BF2_DICE_UTF16(wide)	((const Uint16*) ::gameswf::dice_utf16(wide))
#define BF2_DICE_WIDE(units)	((const wchar_t*) ::gameswf::dice_wide(units))
#else
#define BF2_DICE_UTF16(wide)	(reinterpret_cast<const Uint16*>(wide))
#define BF2_DICE_WIDE(units)	((const wchar_t*) (units))
#endif
	void	array_object_push(as_object* arr, const as_value& v);
	// [CHOSEN, not recovered] S3 bridge for retail's direct Array storage reads.
	// The caller must supply an Array object; no member lookup or conversion runs.
	const array<as_value>& array_object_values(as_object_interface* arr);


	//
	// event_id
	//
	// For keyDown and stuff like that.

	struct event_id
	{
		// These must match the function names in event_id::get_function_name()
		enum id_code
		{
			INVALID,

			// These are for buttons & sprites.
			PRESS,
			RELEASE,
			RELEASE_OUTSIDE,
			ROLL_OVER,
			ROLL_OUT,
			DRAG_OVER,
			DRAG_OUT,
			KEY_PRESS,

			// These are for sprites only.
			INITIALIZE,
			LOAD,
			UNLOAD,
			ENTER_FRAME,
			MOUSE_DOWN,
			MOUSE_UP,
			MOUSE_MOVE,
			KEY_DOWN,
			KEY_UP,
			DATA,
			
			// the SWF 7 `construct` clip event (PlaceObject2 flag bit 18,
			// 0x40000). Flash fires it right after instantiation and BEFORE a registered class's
			// constructor; it is how the authoring tool bakes component-inspector parameters into the
			// movie (`__authorMode = false`, `callbackFrequency = 0`, ...). mainMenu.swf carries 1,178
			// of them. Stock 2006 gameswf had no such event, so every DICE component kept its
			// authoring-mode default and drew its IDE placeholder rectangle - the grey-box menu.
			RESERVED,	// Retail event-name table entry 19.
			CONSTRUCT,
			//  the shipped table's 22nd event, "onDoublePress": a press the
			// host reports with the mouse buttons as 2 (the sprite mouse pass, sub_10080010).
			DOUBLE_PRESS,
			EVENT_COUNT
		};

		unsigned char	m_id;
		unsigned char	m_key_code;

		event_id() : m_id(INVALID), m_key_code(key::INVALID) {}

		// SwiffPlayer.dll 0x1006ED10.
		event_id(id_code id, key::code c = key::INVALID)
			:
			m_id((unsigned char) id),
			m_key_code((unsigned char) c)
		{
			// For the button key events, you must supply a keycode.
			// Otherwise, don't.
			assert((m_key_code == key::INVALID && (m_id != KEY_PRESS))
				|| (m_key_code != key::INVALID && (m_id == KEY_PRESS)));
		}

		// SwiffPlayer.dll 0x1006ED30 compares the event byte before the key byte.
		bool	operator==(const event_id& id) const { return m_id == id.m_id && m_key_code == id.m_key_code; }

		// Return the name of a method-handler function corresponding to this event.
		const tu_string&	get_function_name() const;
	};

	//
	// with_stack_entry
	//
	// The "with" stack is for Pascal-like with-scoping.

	struct with_stack_entry
	{
		// SwiffPlayer.dll 0x10094E8E/0x1008BD83: scope records borrow the
		// receiver. Append, expiry and 0x10082180 cleanup never retain/release it.
		// Cleanup veneer 0x1008BD60 shares that body. Native array destruction
		// frees storage after zeroing its count/capacity; record destruction is trivial.
		as_object_interface*	m_object;
		int	m_block_end_pc;
		
		with_stack_entry()
			:
			m_object(NULL),
			m_block_end_pc(0)
		{
		}

		with_stack_entry(as_object_interface* obj, int end)
			:
			m_object(obj),
			m_block_end_pc(end)
		{
		}
	};


	// Base class for actions.
	struct action_buffer
	{
		action_buffer();
		// SwiffPlayer.dll 0x100766E0: dictionary, then byte storage;
		// 0x100A2650 is the compiler's deleting wrapper.
		~action_buffer() = default;
		void	read(stream* in);
		void	execute(as_environment* env);
		void	execute(
			as_environment* env,
			int start_pc,
			int exec_bytes,
			as_value* retval,
			const array<with_stack_entry>& initial_with_stack,
			bool is_function2);

		bool	is_null()
		{
			return m_buffer.size() < 1 || m_buffer[0] == 0;
		}

		int	get_length() const { return m_buffer.size(); }

	private:
		// Don't put these as values in array<>!  They contain
		// internal pointers and cannot be moved or copied.
		// If you need to keep an array of them, keep pointers
		// to new'd instances.
		action_buffer(const action_buffer& a) { assert(0); }
		void operator=(const action_buffer& a) { assert(0); }

		void	process_decl_dict(int start_pc, int stop_pc);

		// data:
		array<unsigned char>	m_buffer;
		array<const char*>	m_dictionary;
		int	m_decl_dict_processed_at;
	};


	struct fn_call;
	// Execute a definition initialization tag without draining the movie's timeline queue.
	// The tag must have been produced by do_init_action_loader.
	void execute_init_action(execute_tag* tag, as_environment* env);
	typedef void (*as_c_function_ptr)(const fn_call& fn);


	struct as_property_interface
	{
		virtual bool	set_property(int index, const as_value& val) = 0;
	};


	// ActionScript value type.
	struct as_value
	{
		enum type
		{
			UNDEFINED,
			NULLTYPE,
			BOOLEAN,
			STRING,
			NUMBER,
			OBJECT,
			C_FUNCTION,
			AS_FUNCTION,	// ActionScript function.
		};
		type	m_type;
		mutable tu_string	m_string_value;
		union
		{
			bool m_boolean_value;
			// @@ hm, what about PS2, where double is bad?  should maybe have int&float types.
			mutable	double	m_number_value;
			as_object_interface*	m_object_value;
			as_c_function_ptr	m_c_function_value;
			as_as_function*	m_as_function_value;
		};

		as_value()
			:
			m_type(UNDEFINED),
			m_number_value(0.0)
		{
		}

		as_value(const as_value& v)
			:
			m_type(UNDEFINED),
			m_number_value(0.0)
		{
			*this = v;
		}

		as_value(const char* str)
			:
			m_type(STRING),
			m_string_value(str),
			m_number_value(0.0)
		{
		}

		as_value(const wchar_t* wstr)
			:
			m_type(STRING),
			m_string_value(""),
			m_number_value(0.0)
		{
			// Encode the string value as UTF-8.
			//
			// Is this dumb?  Alternatives:
			//
			// 1. store a tu_wstring instead of tu_string?
			// Bloats typical ASCII strings, needs a
			// tu_wstring type, and conversion back the
			// other way to interface with char[].
			// 
			// 2. store a tu_wstring as a union with
			// tu_string?  Extra complexity.
			//
			// 3. ??
			//
			// Storing UTF-8 seems like a pretty decent
			// way to do it.  Everything else just
			// continues to work.

#if !defined(_WIN32) && (WCHAR_MAX > 0xFFFF)
			// [CHOSEN, not recovered] MAXINT is undefined here, so the stock test below picked
			// the 16-bit decoder for the 32-bit POSIX wchar_t. Decode native characters.
			tu_string::encode_utf8_from_wchar(&m_string_value, (const uint32 *)wstr);
#elif (WCHAR_MAX != MAXINT)
			tu_string::encode_utf8_from_wchar(&m_string_value, (const uint16 *)wstr);
#else
# if (WCHAR_MAX != MAXSHORT)
# error "Can't determine the size of wchar_t"
# else
			tu_string::encode_utf8_from_wchar(&m_string_value, (const uint32 *)wstr);
# endif
#endif
		}

		as_value(bool val)
			:
			m_type(BOOLEAN),
			m_boolean_value(val)
		{
		}

		as_value(int val)
			:
			m_type(NUMBER),
			m_number_value(double(val))
		{
		}

		as_value(float val)
			:
			m_type(NUMBER),
			m_number_value(double(val))
		{
		}

		as_value(double val)
			:
			m_type(NUMBER),
			m_number_value(val)
		{
		}

		as_value(as_object_interface* obj);

		as_value(as_c_function_ptr func)
			:
			m_type(C_FUNCTION),
			m_c_function_value(func)
		{
			m_c_function_value = func;
		}

		as_value(as_as_function* func);

		~as_value() { drop_refs(); }

		// Useful when changing types/values.
		void	drop_refs();

		type	get_type() const { return m_type; }

		// Return true if this value is callable.
		bool is_function() const
		{
			return m_type == C_FUNCTION || m_type == AS_FUNCTION;
		}

		const char*	to_string() const;
		const tu_string&	to_tu_string() const;
		const tu_string&	to_tu_string_versioned(int version) const;
		const tu_stringi&	to_tu_stringi() const;
		double	to_number() const;
		bool	to_bool() const;
		as_object_interface*	to_object() const;
		// Extract an OBJECT payload without the native function-property adapter.
		as_object_interface* to_object_raw() const;
		as_c_function_ptr	to_c_function() const;
		as_as_function*	to_as_function() const;

		void	convert_to_number();
		void	convert_to_string();
		void	convert_to_string_versioned(int version);

		// These set_*()'s are more type-safe; should be used
		// in preference to generic overloaded set().  You are
		// more likely to get a warning/error if misused.
		void	set_tu_string(const tu_string& str) { drop_refs(); m_type = STRING; m_string_value = str; }
		// SwiffPlayer.dll 0x1008B130: release before replacing the string payload.
		void	set_string(const char* str) { drop_refs(); m_type = STRING; m_string_value = str; }
		void	set_double(double val) { drop_refs(); m_type = NUMBER; m_number_value = val; }
		void	set_bool(bool val) { drop_refs(); m_type = BOOLEAN; m_boolean_value = val; }
		// SwiffPlayer.dll 0x1006ECB0 repeats the release in set_double; preserve both calls.
		void	set_int(int val) { drop_refs(); set_double(val); }
		void	set_as_object_interface(as_object_interface* obj);
		void	set_as_c_function_ptr(as_c_function_ptr func)
		{
			drop_refs(); m_type = C_FUNCTION; m_c_function_value = func;
		}
		void	set_as_as_function(as_as_function* func);
		void	set_undefined() { drop_refs(); m_type = UNDEFINED; }
		void	set_null() { drop_refs(); m_type = NULLTYPE; }

		void	operator=(const as_value& v)
		{
			if (v.m_type == UNDEFINED) set_undefined();
			else if (v.m_type == NULLTYPE) set_null();
			else if (v.m_type == BOOLEAN) set_bool(v.m_boolean_value);
			else if (v.m_type == STRING) set_tu_string(v.m_string_value);
			else if (v.m_type == NUMBER) set_double(v.m_number_value);
			else if (v.m_type == OBJECT) set_as_object_interface(v.m_object_value);
			else if (v.m_type == C_FUNCTION) set_as_c_function_ptr(v.m_c_function_value);
			else if (v.m_type == AS_FUNCTION) set_as_as_function(v.m_as_function_value);
		}

		bool	operator==(const as_value& v) const;
		bool	operator!=(const as_value& v) const;
		bool	operator<(const as_value& v) const;
		void	operator+=(const as_value& v);
		void	operator-=(const as_value& v);
		void	operator*=(const as_value& v);
		void	operator/=(const as_value& v);
		void	operator&=(const as_value& v);
		void	operator|=(const as_value& v);
		void	operator^=(const as_value& v);
		void	shl(const as_value& v);
		void	asr(const as_value& v);
		void	lsr(const as_value& v);

		void	string_concat(const tu_string& str);

		tu_string* get_mutable_tu_string() { assert(m_type == STRING); return &m_string_value; }
	};


// tulrich: I'm not too sure this is useful.  For things like
// xml_as_object, is it sufficient to always store the event handlers
// as ordinary members using their canonical names, instead of this
// special table?  I have a feeling that's what Macromedia does
// (though I'm not sure).
#if 0
	// This class is just as_object_interface, with an event
	// handler table added.
	struct as_object_with_handlers : public as_object_interface
	{
                // ActionScript event handler table.
                hash<event_id, gameswf::as_value>        m_event_handlers;

                // ActionScript event handler.
                void    set_event_handler(event_id id, const as_value& method)
                {
                        // m_event_handlers.push_back(as);
                        //m_event_handlers.set(id, method);
                }

                bool    get_event_handler(event_id id, gameswf::as_value* result)
                {
                        //return m_event_handlers.get(id, result);
			return false;
                }
	};
#endif // 0


	//
	// as_prop_flags
	//
	// flags defining the level of protection of a member
	struct as_prop_flags
	{
		// Numeric flags
		int m_flags;
		// if true, this value is protected (internal to gameswf)
		bool m_is_protected;

		// mask for flags
		const static int as_prop_flags_mask = 0x7;

		// Default constructor
		as_prop_flags() : m_flags(0), m_is_protected(false)
		{
		}

		// Constructor
		as_prop_flags(const bool read_only, const bool dont_delete, const bool dont_enum)
			:
			m_flags(((read_only) ? 0x4 : 0) | ((dont_delete) ? 0x2 : 0) | ((dont_enum) ? 0x1 : 0)),
			m_is_protected(false)
		{
		}

		// Constructor, from numerical value
		as_prop_flags(const int flags)
			: m_flags(flags), m_is_protected(false)
		{
		}

		// accessor to m_readOnly
		bool get_read_only() const { return (((this->m_flags & 0x4)!=0)?true:false); }

		// accessor to m_dontDelete
		bool get_dont_delete() const { return (((this->m_flags & 0x2)!=0)?true:false); }

		// accessor to m_dontEnum
		bool get_dont_enum() const { return (((this->m_flags & 0x1)!=0)?true:false);	}

		// accesor to the numerical flags value
		int get_flags() const { return this->m_flags; }

		// accessor to m_is_protected
		bool get_is_protected() const { return this->m_is_protected; }
		// setter to m_is_protected
		void set_get_is_protected(const bool is_protected) { this->m_is_protected = is_protected; }

		// set the numerical flags value (return the new value )
		// If unlocked is false, you cannot un-protect from over-write,
		// you cannot un-protect from deletion and you cannot
		// un-hide from the for..in loop construct
		int set_flags(const int setTrue, const int set_false = 0)
		{
			if (!this->get_is_protected())
			{
				this->m_flags = this->m_flags & (~set_false);
				this->m_flags |= setTrue;
			}

			return get_flags();
		}
	};

	//
	// as_member
	//
	// member for as_object: value + flags
	struct as_member {
		// value
		as_value m_value;
		// Properties flags
		as_prop_flags m_flags;

		// Default constructor
		as_member() {
		}

		// Constructor
		as_member(const as_value &value,const as_prop_flags flags=as_prop_flags())
			:
			m_value(value),
			m_flags(flags)
		{
		}

		// accessor to the value
		as_value get_member_value() const { return m_value; }
		// accessor to the properties flags
		as_prop_flags get_member_flags() const { return m_flags; }

		// set the value
		void set_member_value(const as_value &value)  { m_value = value; }
		// accessor to the properties flags
		void set_member_flags(const as_prop_flags &flags)  { m_flags = flags; }
	};


	//
	// as_object
	//
	// A generic bag of attributes.  Base-class for ActionScript
	// script-defined objects.
	// the AS2 `Object.addProperty(name, getter, setter)` builtin. Stock 2006
	// gameswf has no getter/setter properties at all, and menu.swf calls addProperty 36 times in its
	// onLoad (measured with BF2_SWIFFPLAYER_ASLOG=1). DICE hit the same gap: their 56-byte AS object
	// class registers exactly one method, "addProperty" (sub_1009A320 -> gameswf_as_object_addproperty).
	// Defined in gameswf_action.cpp. See GAMESWF_DIVERGENCES.md.
	void	as_object_addproperty(const fn_call& fn);

	struct as_object : public as_object_interface
	{
		stringi_hash<as_member>	m_members;
		// Instance construction and explicit inheritance use different links.
		as_object_interface*	m_instance_prototype;
		as_object_interface*	m_prototype;
		smart_ptr<as_as_function>	m_constructor;

		// SwiffPlayer.dll 0x1009A0F0 stores a pair of borrowed script functions;
		// 0x100749D0 destroys keys without releasing either function.
		struct property_accessors
		{
			as_as_function* getter;
			as_as_function* setter;
		};
		stringi_hash<property_accessors> m_accessors;

		virtual as_object*	cast_to_as_object() { return this; }

		//  The shipped as_object vtable slot +0x28 (sub_10099CB0): push the
		// names a `for (k in obj)` loop visits - the prototype chain first (virtual, so it recurses
		// base-most first), then this object's own members whose VALUE is a string, number, boolean
		// or object (shipped types 1..4); undefined and both function types are skipped, and the
		// shipped member entry carries no flags, so there is no DontEnum test. Arrays override it
		// (sub_1009D020) to push only their element indices. Enumerate (0x46) and Enumerate2 (0x55)
		// call this after pushing the null terminator; stock 2006 walked m_members inline, methods
		// included, so a for-in over an Array met its own push/pop/... C functions.
		virtual void	enumerate(as_environment* env);
		virtual void gc_mark();
		virtual void gc_release();
		void gc_mark_members();


		// Accessors keep the original receiver while searching explicit inheritance links.
		void	add_property(const tu_stringi& name, const as_value& getter, const as_value& setter);
		bool	get_property(const tu_stringi& name, as_value* val, as_object_interface* receiver = NULL);
		bool	set_property(const tu_stringi& name, const as_value& val, as_object_interface* receiver = NULL);
		// a getter/setter registered on a CLASS PROTOTYPE applies to every
		// instance, called with the instance as `this`. mainMenu.swf registers 177 of them on the
		// component prototypes (`UIObject.prototype.addProperty("__authorMode", __get__..., __set__...)`)
		// and the construct handlers then assign `__authorMode = false` on the instance. The lookup
		// walks the prototype chain; the call keeps the original receiver.
		bool	find_getter(const tu_stringi& name, as_value* getter, int depth = 0);
		bool	find_setter(const tu_stringi& name, as_value* setter, int depth = 0);
		static as_value	invoke_getter(const as_value& getter, as_object_interface* self,
			const as_value& argument = as_value());
		static void	invoke_setter(const as_value& setter, as_object_interface* self, const as_value& val);

		as_object();
		as_object(as_object_interface* proto);
		// Compiler deleting wrapper: SwiffPlayer.dll 0x1007CD80. Native base
		// and member ownership remain separate from this forwarding/deletion sequence.
		virtual ~as_object();
		
		virtual const char*	get_text_value() const { return NULL; }

		virtual void	set_member(const tu_stringi& name, const as_value& val);
		virtual bool	get_member(const tu_stringi& name, as_value* val);

		virtual bool get_member(const tu_stringi& name, as_member* member) const
		{
			//printf("GET MEMBER: %s at %p for object %p\n", name.c_str(), val, this);
			assert(member != NULL);
			return m_members.get(name, member);
		}

		virtual bool	set_member_flags(const tu_stringi& name, const int flags)
		{
			as_member member;
			if (this->get_member(name, &member)) {
				as_prop_flags f = member.get_member_flags();
				f.set_flags(flags);
				member.set_member_flags(f);

				m_members.set(name, member);

				return true;
			}

			return false;
		}

		virtual movie*	to_movie()
		// This object is not a movie; no conversion.
		{
			return NULL;
		}

		void	clear();
	};


	//
	// as_as_function
	//
	// ActionScript function.

	struct as_as_function : public ref_counted
	{
		action_buffer*	m_action_buffer;
		as_environment*	m_env;	// @@ might need some kind of ref count here, but beware cycles
		array<with_stack_entry>	m_with_stack;	// initial with-stack on function entry.
		int	m_start_pc;
		int	m_length;
		struct arg_spec
		{
			int	m_register;
			tu_string	m_name;
		};
		array<arg_spec>	m_args;
		bool	m_is_function2;
		uint8	m_local_register_count;	// parsed; sizes nothing since #55 (the shipped call saves a fixed r0..r9)
		uint16	m_function2_flags;	// retained for the native declaration interface
		// SwiffPlayer.dll 0x10091C20 / 0x100958FB: register number to preload name.
		hash<int, tu_string>	m_preload_registers;

		// [CHOSEN, not recovered] S3 bridge: native object-interface adapter for the
		// function's static properties and its eagerly created prototype object.
		as_object*	m_properties;

		// NULL environment is allowed -- if so, then
		// functions will be executed in the caller's
		// environment, rather than the environment where they
		// were defined.
		as_as_function(action_buffer* ab, as_environment* env, int start, const array<with_stack_entry>& with_stack);

		// SwiffPlayer.dll 0x10091BB0 / 0x10091BE0 destroys static members before
		// arguments; 0x10091C60 destroys the separate function2 storage first.
		// [CHOSEN, not recovered] S3 bridge: this combined native type owns the
		// property adapter and preload map. Release its property reference for
		// issue #7 (https://github.com/B2BF/OpenBF2/issues/7).
		~as_as_function()
		{
			if (m_properties)
			{
				m_properties->drop_ref();
			}
		}

		// SwiffPlayer.dll 0x10091C20 starts with empty preload/argument storage.
		// [CHOSEN, not recovered] S3 bridge: the native type combines both function classes.
		void	set_is_function2() { m_is_function2 = true; }
		void	set_local_register_count(uint8 ct) { assert(m_is_function2); m_local_register_count = ct; }
		void	set_function2_flags(uint16 flags);

		void	add_arg(int arg_register, const char* name)
		{
			assert(arg_register == 0 || m_is_function2 == true);
			m_args.resize(m_args.size() + 1);
			m_args.back().m_register = arg_register;
			m_args.back().m_name = name;
		}

		void	add_arg(const char* name)
		{
			// SwiffPlayer.dll 0x1008D610 copies the name before growing the array.
			// [CHOSEN, not recovered] S3 bridge: the native argument record also
			// carries the register used by the separate function2 class.
			const arg_spec argument = { 0, tu_string(name) };
			m_args.push_back(argument);
		}

		void	set_length(int len) { assert(len >= 0); m_length = len; }

		// Dispatch.
		void	operator()(const fn_call& fn);

		void	lazy_create_properties();
		void gc_mark();
	};


	// ActionScript "environment", essentially VM state?
	struct as_environment
	{
		array<as_value>	m_stack;
		//  ONE register bank per environment, 256 slots, shared by timeline
		// code and every function body that executes in this environment - the shipped
		// as_environment carries `as_value registers[256]` at +0x10 (ctor 0x10077D00), and
		// StoreRegister / the push-register operand address env+0x10+32*reg for all code (execute
		// 0x100925A0, case 0x87 and push type 4). Stock 2006 kept four timeline registers plus a
		// FRESH stack of local registers per DefineFunction2 call, so an argument the caller did not
		// pass read undefined; in the shipped player it reads whatever the environment's register
		// already held (see as_as_function::operator()). Constructed with 256 entries.
		// [CHOSEN, not recovered] S3 bridge: native array storage for the fixed bank.
		array<as_value>	m_registers;
		movie*	m_target;
		stringi_hash<as_value>	m_variables;

		// For local vars.  Use empty names to separate frames.
		struct frame_slot
		{
			tu_string	m_name;
			as_value	m_value;

			frame_slot() {}
			// SwiffPlayer.dll 0x10071420: implicit destruction releases the
			// value before the name, including when a local-frame resize shrinks.
			// SwiffPlayer.dll 0x1008C460: copy the name, then the value.
			frame_slot(const tu_string& name, const as_value& val) : m_name(name), m_value(val) {}
			// SwiffPlayer.dll 0x1008BDA0: the implicit copy follows the same member order.
		};
		array<frame_slot>	m_local_frames;

		// the running function's bound `this` object and its superclass
		// constructor (`super`). The shipped as_environment keeps them at +8212 (checked by
		// get_variable_raw before the target movie's members, used as `this` by CallMethod's
		// direct-call arm) and +8216 (preloaded into the `super` register/local at function entry).
		// Stock 2006 bound `this` to the target MOVIE for every call and never implemented `super`,
		// so a class constructor run on a plain object or another sprite acted on the wrong object
		// and super() went nowhere. Non-owning; saved/restored around each call.
		as_object_interface*	m_this_object;
		as_as_function*		m_super_ctor;

		//  While a function body defined in this environment runs for
		// ANOTHER environment, that caller's register bank; NULL otherwise (this environment's own).
		// Set and restored around each call by as_as_function::operator(). Non-owning.
		as_environment*	m_register_env;

		// SwiffPlayer.dll 0x10077D00 constructs all 256 register values eagerly.
		as_environment()
			:
			m_target(0),
			m_this_object(0),
			m_super_ctor(0),
			m_register_env(0)
		{
			m_registers.resize(256);
		}
		~as_environment();

		// SwiffPlayer.dll 0x1008A110; native movie pointer adjusts the secondary interface.
		movie*	get_target() { return m_target; }
		// SwiffPlayer.dll 0x1006ECE0 writes the target after the fixed register bank.
		void	set_target(movie* target) { m_target = target; }
		// SwiffPlayer.dll 0x1008A120 / 0x1008A140; names [INFERRED].
		void set_this_object(as_object_interface* object) { m_this_object = object; }
		as_as_function* get_super_constructor() const { return m_super_ctor; }

		// stack access/manipulation
		// @@ TODO do more checking on these
		// SwiffPlayer.dll push helpers: 0x1008E0F0 (string), 0x1008E180 (double),
		// 0x1008E200 (float), 0x1008E3C0 (Boolean), 0x1008E450 (signed integer).
		// 0x1008E280 / 0x1008E320 also construct an OBJECT before growing storage.
		// 0x100A8CC0 constructs a narrow C-string value before resize/assignment;
		// XML enumeration inlines the same sequence at 0x100A8D83..0x100A8DD1.
		// [INFERRED] C-string instantiation of this template; native string storage.
		// [CHOSEN, not recovered] S3 bridge: native objects share one refcount interface.
		// Construct the value before growing the operand storage, then append a copy.
		template<class T>
		void	push(T val)
		{
			const as_value value(val);
			m_stack.push_back(value);
		}
		void	push_val(as_value val)
		{
			// SwiffPlayer.dll 0x10077640 copies its by-value argument into a local
			// before resizing. Both copies stay alive through the appended assignment.
			as_value copy;
			copy = val;
			m_stack.push_back(copy);
		}
		// [CHOSEN, not recovered] S3 bridge: retain the native menu integration's
		// tolerant operand accesses while the VM/base boundary remains incomplete.
		// Retail top (0x1008AB50), pop (0x10076600) and drop (0x1008C4A0) do not
		// clamp; the shared resize helper (0x1000B980) traps on a negative size.
		// [RETIRE-WHEN: swf-s3-operand-underflow] Reconcile native/retail operand
		// effects with the base tag and validate menu initialization, callbacks and
		// malformed stacks before replacing these guards. Empty accesses currently
		// reached by the native menus are not proof of retail underflow behavior.
		static as_value&	underflow_slot() { static as_value s_undef; s_undef.set_undefined(); return s_undef; }

		as_value	pop()
		{
			// SwiffPlayer.dll 0x10076600 copies the top before shrinking;
			// the empty-stack return is the native tolerance bridge above.
			if (m_stack.size() <= 0) return as_value();	// undefined
			as_value result = m_stack.back(); m_stack.pop_back(); return result;
		}
		as_value&	top(int dist)
		{
			int i = m_stack.size() - 1 - dist;
			if (i < 0 || i >= m_stack.size()) return underflow_slot();
			return m_stack[i];
		}
		as_value&	bottom(int index)
		{
			if (index < 0 || index >= m_stack.size()) return underflow_slot();
			return m_stack[index];
		}
		// Keep an excessive drop within the native operand storage; see the bridge above.
		void	drop(int count) { int n = m_stack.size() - count; if (n < 0) n = 0; m_stack.resize(n); }

		int	get_top_index() const { return m_stack.size() - 1; }

		// A successful with-scope lookup also writes its object to receiver, when supplied.
		as_value	get_variable(const tu_string& varname, const array<with_stack_entry>& with_stack,
			as_object_interface** receiver = NULL) const;
		// no path stuff:
		as_value	get_variable_raw(const tu_string& varname, const array<with_stack_entry>& with_stack,
			as_object_interface** receiver = NULL) const;

		void	set_variable(const tu_string& path, const as_value& val, const array<with_stack_entry>& with_stack);
		// no path stuff:
		void	set_variable_raw(const tu_string& path, const as_value& val, const array<with_stack_entry>& with_stack);

		void	set_local(const tu_string& varname, const as_value& val);
		void	add_local(const tu_string& varname, const as_value& val);	// when you know it doesn't exist.
		void	declare_local(const tu_string& varname);	// Declare varname; undefined unless it already exists.

		bool	get_member(const tu_stringi& varname, as_value* val) const;
		void	set_member(const tu_stringi& varname, const as_value& val);

		// Parameter/local stack frame management.
		// SwiffPlayer.dll 0x1008AB70 returns the local-frame count.
		int	get_local_frame_top() const { return m_local_frames.size(); }
		// SwiffPlayer.dll 0x1008C4C0 forwards to the local-frame resize helper.
		void	set_local_frame_top(int t) { assert(t <= m_local_frames.size()); m_local_frames.resize(t); }
		void	add_frame_barrier() { m_local_frames.push_back(frame_slot()); }

		//  the environment's register `reg` (the operand is a byte, so
		// 0..255 is always a valid slot). Never NULL.
		as_value*	register_ptr(int reg);
		//  the environment whose bank register operands address here.
		as_environment*	register_bank() { return m_register_env ? m_register_env : this; }

		// Internal.
		int	find_local(const tu_string& varname) const;
		bool	parse_path(const tu_string& var_path, tu_string* path, tu_string* var) const;
		movie*	find_target(const tu_string& path) const;
		as_object_interface*	find_target(const as_value& val) const;
		// dotted-path resolution through OBJECT members, with the shipped
		// _global fallback - the resolver get_variable/set_variable use. See the .cpp.
		as_object_interface*	find_object_path(const tu_string& path, as_object_interface* start) const;

		// Movie teardown: appends every object or function held on the stack, in the registers,
		// the variables and the local frames to *held, then empties them.
		// [CHOSEN, not recovered] issue #7; see release_script_objects.
		void	release_values(array<as_value>* held);
	};


	// Appends v to *held when it is an object or an ActionScript function, the only values that
	// can keep another object alive. A helper of the movie teardown (release_script_objects).
	// [CHOSEN, not recovered] issue #7.
	inline void	hold_script_value(array<as_value>* held, const as_value& v)
	{
		if (v.get_type() == as_value::OBJECT || v.get_type() == as_value::AS_FUNCTION)
		{
			held->push_back(v);
		}
	}

	// Breaks every script reference cycle between the live script objects, so plain reference
	// counting can free them once their owners let go. Every registered clip and object (see
	// register_script_object), reachable from the movie or not, is cleared: a clip's environment,
	// event handlers and children (without UNLOAD events), an object's members, accessors and
	// prototype. Runs no ActionScript. Afterwards every script object is unusable and _global is
	// empty: call it only when the whole player is going away, right before its movie's last
	// reference is dropped and _global is reset, and only when no other player is alive
	// (SwiffPlayer::releaseMovie counts them), so nothing still in use is cleared.
	// [CHOSEN, not recovered] issue #7 (https://github.com/B2BF/OpenBF2/issues/7): our gameswf
	// replaces the shipped player, whose own teardown is not recovered.
	void	release_script_objects();

	// Drops the Object.registerClass bindings of every library definition (gameswf_impl.cpp).
	// Part of release_script_objects. [CHOSEN, not recovered] issue #7.
	void	release_library_classes();


	// Parameters/environment for C functions callable from ActionScript.
	struct fn_call
	{
		as_value* result;
		as_object_interface* this_ptr;
		as_environment* env;
		int nargs;
		int first_arg_bottom_index;

		fn_call(as_value* res_in, as_object_interface* this_in, as_environment* env_in, int nargs_in, int first_in)
			:
			result(res_in),
			this_ptr(this_in),
			env(env_in),
			nargs(nargs_in),
			first_arg_bottom_index(first_in)
		{
		}

		as_value& arg(int n) const
		// Access a particular argument.
		{
			assert(n < nargs);
			return env->bottom(first_arg_bottom_index - n);
		}
	};


	//
	// Some handy helpers
	//

	// Clean up any stray heap stuff we've allocated.
	void	action_clear();
	// [CHOSEN, not recovered] S3 bridge for the borrowed constructor context
	// used by the separately compiled timer dispatcher.
	void set_call_super(as_as_function* constructor);
	void set_environment_super(as_environment* env);
	void gc_mark_value(const as_value& value);
	void gc_mark_modules();
	// [CHOSEN, not recovered] S3 bridge: expose mark traversal independently
	// while XML edges, host roots and native strong ownership block the sweep.
	// This advances only the generation; it never claims to collect objects.
	void advance_gc_mark_generation();

	// Dispatching methods from C++.
	as_value	call_method0(const as_value& method, as_environment* env, as_object_interface* this_ptr);
	as_value	call_method1(
		const as_value& method, as_environment* env, as_object_interface* this_ptr,
		const as_value& arg0);
	as_value	call_method2(
		const as_value& method, as_environment* env, as_object_interface* this_ptr,
		const as_value& arg0, const as_value& arg1);
	as_value	call_method3(
		const as_value& method, as_environment* env, as_object_interface* this_ptr,
		const as_value& arg0, const as_value& arg1, const as_value& arg2);


	// Calls an expression containing a method name and quoted or numeric arguments.
	// Returns shared string storage overwritten by the next expression call.
	// The environment must have a target and the expression at least one token.
	// Parsing uses at most 999 bytes.
	const char*	call_method_expression(as_environment* env, as_object_interface* receiver,
		const char* expression);
	const char*	call_method_expression(as_environment* env, as_object_interface* receiver,
		const Uint16* expression);

	// Access the independent Key module without initializing it. Missing modules are ignored.
	void	add_key_listener(as_object_interface* listener);
	void	remove_key_listener(as_object_interface* listener);
	int	get_last_key_pressed();

	// tulrich: don't use this!  To register a class constructor,
	// just assign the classname to the constructor function.  E.g.:
	//
	// my_movie->set_member("MyClass", as_value(MyClassConstructorFunction));
	// 
	//void register_as_object(const char* object_name, as_c_function_ptr handler);

	// accessor for the _global object, so the SwiffPlayer AS bridge can hang
	// the dice.* namespace graph off it before the first advance. Forces action_init(). See
	// gameswf_action.cpp and GAMESWF_DIVERGENCES.md.
	as_object*	get_global_object();

	// Numerical indices for standard member names.  Can use this
	// to help speed up get/set member calls, by using a switch()
	// instead of nasty string compares.
	enum as_standard_member
	{
		M_INVALID_MEMBER = -1,
		M_X,
		M_Y,
		M_XSCALE,
		M_YSCALE,
		M_CURRENTFRAME,
		M_TOTALFRAMES,
		M_ALPHA,
		M_VISIBLE,
		M_WIDTH,
		M_HEIGHT,
		M_ROTATION,
		M_TARGET,
		M_FRAMESLOADED,
		M_NAME,
		M_DROPTARGET,
		M_URL,
		M_HIGHQUALITY,
		M_FOCUSRECT,
		M_SOUNDBUFTIME,
		M_XMOUSE,
		M_YMOUSE,
		M_PARENT,
		M_TEXT,
		M_TEXTCOLOR,
		M_ONLOAD,

		AS_STANDARD_MEMBER_COUNT
	};
	// Return the standard enum, if the arg names a standard member.
	// Returns M_INVALID_MEMBER if there's no match.
	as_standard_member	get_standard_member(const tu_stringi& name);

}	// end namespace gameswf


#endif // GAMESWF_ACTION_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
