// gameswf_action.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Implementation and helpers for SWF actions.


#include "gameswf_action.h"
// [TEMP] RW-89 diagnostic: the last member name ActionGetMember read, so the two 'invalid object'
// errors below can say WHICH member came back undefined. Remove with the tracers.
static tu_string	s_last_member_name;
#include "gameswf_impl.h"
#include "gameswf_log.h"
#include "gameswf_stream.h"
#include "base/tu_random.h"

#include "gameswf_string.h"
#include "gameswf_movie.h"
#include "gameswf_timers.h"
#include "gameswf_textformat.h"
#include "gameswf_sound.h"

#ifdef HAVE_LIBXML
#include "gameswf_xml.h"
#include "gameswf_xmlsocket.h"
#endif


#ifdef _WIN32
#define snprintf _snprintf
#endif // _WIN32

// NOTES:
//
// Buttons
// on (press)                 onPress
// on (release)               onRelease
// on (releaseOutside)        onReleaseOutside
// on (rollOver)              onRollOver
// on (rollOut)               onRollOut
// on (dragOver)              onDragOver
// on (dragOut)               onDragOut
// on (keyPress"...")         onKeyDown, onKeyUp      <----- IMPORTANT
//
// Sprites
// onClipEvent (load)         onLoad
// onClipEvent (unload)       onUnload                Hm.
// onClipEvent (enterFrame)   onEnterFrame
// onClipEvent (mouseDown)    onMouseDown
// onClipEvent (mouseUp)      onMouseUp
// onClipEvent (mouseMove)    onMouseMove
// onClipEvent (keyDown)      onKeyDown
// onClipEvent (keyUp)        onKeyUp
// onClipEvent (data)         onData

// Text fields have event handlers too!

// Sprite built in methods:
// play()
// stop()
// gotoAndStop()
// gotoAndPlay()
// nextFrame()
// startDrag()
// getURL()
// getBytesLoaded()
// getBytesTotal()

// Built-in functions: (do these actually exist in the VM, or are they just opcodes?)
// Number()
// String()


// TODO builtins
//
// Number.toString() -- takes an optional arg that specifies the base
//
// parseInt(), parseFloat()
//
// Boolean() type cast
//
// typeof operator --> "number", "string", "boolean", "object" (also
// for arrays), "null", "movieclip", "function", "undefined"
//
// isNaN()
//
// Number.MAX_VALUE, Number.MIN_VALUE
//
// String.fromCharCode()



namespace gameswf
{
	
	//
	// action stuff
	//

	// [DICE-MOD SwiffPlayer #40] the Number and Boolean wrapper objects the shipped action_init
	// (sub_10090FF0) registers beside String/Array/Object (sub_1008C520: a 68-byte object holding
	// the double at +48; sub_1008C4D0: a 60-byte object holding the bool at +44; each constructor
	// takes an optional first argument and answers the object). Their text value is the value's
	// own string form, which is what to_number()/to_tu_string() read from an object - so
	// `new Number(row.selected) > 0` in the DICE ColumnList's fill compares the number, where
	// stock 2006 (no Number constructor) yielded undefined and never preselected a row.
	struct number_object : public as_object
	{
		double	m_value;
		tu_string	m_text;
		number_object(double v) : m_value(v) { m_text = as_value(v).to_tu_string(); }
		virtual const char*	get_text_value() const { return m_text.c_str(); }
	};
	struct boolean_object : public as_object
	{
		bool	m_value;
		boolean_object(bool v) : m_value(v) {}
		virtual const char*	get_text_value() const { return m_value ? "true" : "false"; }
	};
	void	as_global_number_ctor(const fn_call& fn)
	{
		number_object*	obj = new number_object(fn.nargs > 0 ? fn.arg(0).to_number() : 0.0);
		fn.result->set_as_object_interface(obj);
	}
	void	as_global_boolean_ctor(const fn_call& fn)
	{
		boolean_object*	obj = new boolean_object(fn.nargs > 0 ? fn.arg(0).to_bool() : false);
		fn.result->set_as_object_interface(obj);
	}

	void	action_init();

	// Statics.
	bool	s_inited = false;
	smart_ptr<as_object>	s_global;
	fscommand_callback	s_fscommand_handler = NULL;


#define EXTERN_MOVIE
	
#ifdef EXTERN_MOVIE
	void attach_extern_movie(const char* url, const movie* target, const movie* root_movie)
	{
		tu_string infile = get_workdir();
		infile += url;

		movie_definition_sub*	md = create_library_movie_sub(infile.c_str());
		if (md == NULL)
		{
			log_error("can't create movie_definition_sub for %s\n", infile.c_str());
			return;
		}

		gameswf::movie_interface* extern_movie;

		if (target == root_movie)
		{
			extern_movie = create_library_movie_inst_sub(md);			
			if (extern_movie == NULL)
			{
				log_error("can't create extern root movie_interface for %s\n", infile.c_str());
				return;
			}
			set_current_root(extern_movie);
			movie* m = extern_movie->get_root_movie();

			m->on_event(event_id::LOAD);
		}
		else
		{
			extern_movie = md->create_instance();
			if (extern_movie == NULL)
			{
				log_error("can't create extern movie_interface for %s\n", infile.c_str());
				return;
			}
      
			save_extern_movie(extern_movie);
      
			character* tar = (character*)target;
			const char* name = tar->get_name();
			Uint16 depth = tar->get_depth();
			bool use_cxform = false;
			cxform color_transform =  tar->get_cxform();
			bool use_matrix = false;
			matrix mat = tar->get_matrix();
			float ratio = tar->get_ratio();
			Uint16 clip_depth = tar->get_clip_depth();

			movie* parent = tar->get_parent();
			movie* new_movie = extern_movie->get_root_movie();

			assert(parent != NULL);

			((character*)new_movie)->set_parent(parent);
       
			parent->replace_display_object(
				(character*) new_movie,
				name,
				depth,
				use_cxform,
				color_transform,
				use_matrix,
				mat,
				ratio,
				clip_depth);
		}
	}
#endif // EXTERN_MOVIE

	void	register_fscommand_callback(fscommand_callback handler)
	// External interface.
	{
		s_fscommand_handler = handler;
	}



	static bool string_to_number(double* result, const char* str)
	// Utility.  Try to convert str to a number.  If successful,
	// put the result in *result, and return true.  If not
	// successful, put 0 in *result, and return false.
	{
		char* tail = 0;
		*result = strtod(str, &tail);
		if (tail == str || *tail != 0)
		{
			// Failed conversion to Number.
			return false;
		}
		return true;
	}


	//
	// array object
	//


	// [DICE-MOD SwiffPlayer: the AS Array methods. Stock 2006 had NONE (its constructor was commented
	// out, "array methods not implemented yet"), and the menu's list code stops at its first `push`
	// (retail 2026-09-02). The shipped array object (sub_1009CB20, 68 bytes) registers concat, join,
	// pop, push, reverse, shift, slice, sort, splice, toString, unshift on every instance; `length`
	// is live. Elements keep the stock member-per-index representation ("0", "1", ...), with a
	// tracked length so holes and explicit `length` writes behave.]
	void	array_push(const fn_call& fn);
	void	array_pop(const fn_call& fn);
	void	array_shift(const fn_call& fn);
	void	array_unshift(const fn_call& fn);
	void	array_join(const fn_call& fn);
	void	array_to_string(const fn_call& fn);
	void	array_reverse(const fn_call& fn);
	void	array_slice(const fn_call& fn);
	void	array_splice(const fn_call& fn);
	void	array_concat(const fn_call& fn);
	void	array_sort(const fn_call& fn);

	// [DICE-MOD SwiffPlayer #49] sub_10099CB0 - see the declaration in gameswf_action.h. The
	// prototype is reached the way the two enumerate opcodes always cast it; then the own members,
	// filtered on the value's type exactly as the shipped loop tests `1 <= type <= 4` (its enum is
	// STRING 1, NUMBER 2, BOOLEAN 3, OBJECT 4, C_FUNCTION 5, AS_FUNCTION 6; null is an OBJECT there,
	// so this port's NULLTYPE passes too).
	void	as_object::enumerate(as_environment* env)
	{
		as_object*	proto = m_prototype ? m_prototype->cast_to_as_object() : NULL;
		if (proto)
		{
			proto->enumerate(env);
		}
		for (stringi_hash<as_member>::const_iterator it = m_members.begin(); it != m_members.end(); ++it)
		{
			const as_value::type	t = it.get_value().get_member_value().get_type();
			if (t == as_value::STRING || t == as_value::NUMBER || t == as_value::BOOLEAN
			    || t == as_value::OBJECT || t == as_value::NULLTYPE)
			{
				env->push(as_value(it.get_key()));
			}
		}
	}

	struct as_array_object : public as_object
	{
		int	m_length;

		as_array_object()
			:
			m_length(0)
		{
			this->set_member("push", &array_push);
			this->set_member("pop", &array_pop);
			this->set_member("shift", &array_shift);
			this->set_member("unshift", &array_unshift);
			this->set_member("join", &array_join);
			this->set_member("toString", &array_to_string);
			this->set_member("reverse", &array_reverse);
			this->set_member("slice", &array_slice);
			this->set_member("splice", &array_splice);
			this->set_member("concat", &array_concat);
			this->set_member("sort", &array_sort);
		}

		// [DICE-MOD SwiffPlayer #49] sub_1009D020 - the array's own enumerate: the element indices
		// "0".."length-1" and nothing else. Neither the prototype nor m_members (where this port keeps
		// push/pop/... and the elements themselves) is visited, so `for (k in group)` in mainMenu.swf's
		// Radio.toggleCheck sees only its sibling buttons.
		virtual void	enumerate(as_environment* env)
		{
			for (int i = 0; i < m_length; i++)
			{
				char	buf[16];
				sprintf(buf, "%d", i);
				env->push(as_value(buf));
			}
		}

		static tu_stringi	index_name(int i)
		{
			as_value	k;
			k.set_int(i);
			return tu_stringi(k.to_string());
		}

		as_value	get_element(int i)
		{
			as_value	v;
			as_object::get_member(index_name(i), &v);
			return v;
		}

		void	set_element(int i, const as_value& v)
		{
			as_object::set_member(index_name(i), v);
			if (i >= m_length)
			{
				m_length = i + 1;
			}
		}

		virtual bool	get_member(const tu_stringi& name, as_value* val)
		{
			if (name == "length")
			{
				val->set_int(m_length);
				return true;
			}
			return as_object::get_member(name, val);
		}

		virtual void	set_member(const tu_stringi& name, const as_value& val)
		{
			if (name == "length")
			{
				int	n = (int) val.to_number();
				if (n < 0) n = 0;
				for (int i = n; i < m_length; i++)
				{
					as_object::set_member(index_name(i), as_value());
				}
				m_length = n;
				return;
			}
			const char*	s = name.c_str();
			if (s[0] >= '0' && s[0] <= '9')
			{
				const int	idx = atoi(s);
				if (idx >= m_length)
				{
					m_length = idx + 1;
				}
			}
			as_object::set_member(name, val);
		}
	};

	// [DICE-MOD SwiffPlayer #31] see gameswf_action.h.
	as_object*	create_array_object()
	{
		return new as_array_object;
	}

	void	array_object_push(as_object* arr, const as_value& v)
	{
		as_array_object*	a = static_cast<as_array_object*>(arr);
		a->set_element(a->m_length, v);
	}

	static as_array_object*	array_this(const fn_call& fn)
	{
		as_object*	o = fn.this_ptr ? fn.this_ptr->cast_to_as_object() : NULL;
		// The shipped bodies take `this` for an array unconditionally; so does this cast.
		return o ? static_cast<as_array_object*>(o) : NULL;
	}

	void	array_push(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		for (int i = 0; i < fn.nargs; i++)
		{
			a->set_element(a->m_length, fn.arg(i));
		}
		fn.result->set_int(a->m_length);
	}

	void	array_pop(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL || a->m_length <= 0) { *fn.result = as_value(); return; }
		*fn.result = a->get_element(a->m_length - 1);
		as_object* base = a;
		base->set_member(as_array_object::index_name(a->m_length - 1), as_value());
		a->m_length--;
	}

	void	array_shift(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL || a->m_length <= 0) { *fn.result = as_value(); return; }
		*fn.result = a->get_element(0);
		for (int i = 1; i < a->m_length; i++)
		{
			a->set_element(i - 1, a->get_element(i));
		}
		as_object* base = a;
		base->set_member(as_array_object::index_name(a->m_length - 1), as_value());
		a->m_length--;
	}

	void	array_unshift(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		const int	n = fn.nargs;
		for (int i = a->m_length - 1; i >= 0; i--)
		{
			a->set_element(i + n, a->get_element(i));
		}
		for (int i = 0; i < n; i++)
		{
			a->set_element(i, fn.arg(i));
		}
		fn.result->set_int(a->m_length);
	}

	static void	array_join_impl(as_array_object* a, const char* sep, as_value* result)
	{
		tu_string	s;
		for (int i = 0; i < a->m_length; i++)
		{
			if (i > 0) s += sep;
			s += a->get_element(i).to_tu_string();
		}
		result->set_tu_string(s);
	}

	void	array_join(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		tu_string	sep = ",";
		if (fn.nargs > 0) sep = fn.arg(0).to_tu_string();
		array_join_impl(a, sep.c_str(), fn.result);
	}

	void	array_to_string(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		array_join_impl(a, ",", fn.result);
	}

	void	array_reverse(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		for (int i = 0, j = a->m_length - 1; i < j; i++, j--)
		{
			as_value	t = a->get_element(i);
			a->set_element(i, a->get_element(j));
			a->set_element(j, t);
		}
		fn.result->set_as_object_interface(a);
	}

	void	array_slice(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		int	start = fn.nargs > 0 ? (int) fn.arg(0).to_number() : 0;
		int	end = fn.nargs > 1 ? (int) fn.arg(1).to_number() : a->m_length;
		if (start < 0) start += a->m_length;
		if (end < 0) end += a->m_length;
		if (start < 0) start = 0;
		if (end > a->m_length) end = a->m_length;
		smart_ptr<as_array_object>	r = new as_array_object;
		for (int i = start; i < end; i++)
		{
			r->set_element(r->m_length, a->get_element(i));
		}
		fn.result->set_as_object_interface(r.get_ptr());
	}

	void	array_splice(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		int	start = fn.nargs > 0 ? (int) fn.arg(0).to_number() : 0;
		if (start < 0) start += a->m_length;
		if (start < 0) start = 0;
		if (start > a->m_length) start = a->m_length;
		int	del = fn.nargs > 1 ? (int) fn.arg(1).to_number() : a->m_length - start;
		if (del < 0) del = 0;
		if (start + del > a->m_length) del = a->m_length - start;
		const int	ins = fn.nargs > 2 ? fn.nargs - 2 : 0;

		smart_ptr<as_array_object>	removed = new as_array_object;
		for (int i = 0; i < del; i++)
		{
			removed->set_element(i, a->get_element(start + i));
		}
		// Shift the tail by (ins - del).
		const int	old_len = a->m_length;
		const int	tail = old_len - (start + del);
		if (ins > del)
		{
			for (int i = tail - 1; i >= 0; i--)
			{
				a->set_element(start + ins + i, a->get_element(start + del + i));
			}
		}
		else if (ins < del)
		{
			for (int i = 0; i < tail; i++)
			{
				a->set_element(start + ins + i, a->get_element(start + del + i));
			}
			as_object* base = a;
			for (int i = old_len - (del - ins); i < old_len; i++)
			{
				base->set_member(as_array_object::index_name(i), as_value());
			}
			a->m_length = old_len - (del - ins);
		}
		for (int i = 0; i < ins; i++)
		{
			a->set_element(start + i, fn.arg(2 + i));
		}
		fn.result->set_as_object_interface(removed.get_ptr());
	}

	void	array_concat(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		smart_ptr<as_array_object>	r = new as_array_object;
		for (int i = 0; i < a->m_length; i++)
		{
			r->set_element(r->m_length, a->get_element(i));
		}
		for (int k = 0; k < fn.nargs; k++)
		{
			as_object*	o = fn.arg(k).to_object() ? fn.arg(k).to_object()->cast_to_as_object() : NULL;
			as_value	len;
			if (o && o->get_member("length", &len) && len.get_type() == as_value::NUMBER
			    && o->get_member("push", &len))
			{
				// another array: flatten one level
				as_array_object*	b = static_cast<as_array_object*>(o);
				for (int i = 0; i < b->m_length; i++)
				{
					r->set_element(r->m_length, b->get_element(i));
				}
			}
			else
			{
				r->set_element(r->m_length, fn.arg(k));
			}
		}
		fn.result->set_as_object_interface(r.get_ptr());
	}

	void	array_sort(const fn_call& fn)
	{
		as_array_object*	a = array_this(fn);
		if (a == NULL) return;
		if (fn.nargs > 0 && (fn.arg(0).to_as_function() || fn.arg(0).to_c_function()))
		{
			log_error("Array.sort: a compare function is not supported yet; sorting as strings\n");
		}
		// Insertion sort on the string forms (the AS default ordering).
		for (int i = 1; i < a->m_length; i++)
		{
			as_value	key = a->get_element(i);
			const tu_string	ks = key.to_tu_string();
			int	j = i - 1;
			while (j >= 0 && a->get_element(j).to_tu_string() > ks)
			{
				a->set_element(j + 1, a->get_element(j));
				j--;
			}
			a->set_element(j + 1, key);
		}
		fn.result->set_as_object_interface(a);
	}


	//
	// as_as_function
	//

	void	as_as_function::operator()(const fn_call& fn)
	// Dispatch.
	{
		as_environment*	our_env = m_env;
		if (our_env == NULL)
		{
			our_env = fn.env;
		}
		assert(our_env);

		// [DICE-MOD SwiffPlayer: bind `this` and `super` for this call. `this` is the object the
		// caller passed (the shipped dispatch, sub_10095840, preloads its a3 argument), falling
		// back to the target movie only when none was given. `super` is this function's own
		// superclass constructor - the __constructor__ that ActionExtends left on its prototype -
		// which is what the shipped CallMethod hands the callee when a function value is called
		// directly (super()). Both live in the environment for the duration of the call, restored
		// on every exit path.]
		as_object_interface*	this_obj = fn.this_ptr ? fn.this_ptr : static_cast<as_object_interface*>(our_env->m_target);
		as_as_function*	super_fn = NULL;
		if (m_properties)
		{
			// A class constructor: its own prototype carries the __constructor__ ActionExtends left.
			as_value	proto_val;
			m_properties->get_member("prototype", &proto_val);
			as_object_interface*	proto = proto_val.to_object();
			if (proto)
			{
				as_value	ctor_val;
				if (proto->get_member("__constructor__", &ctor_val))
				{
					super_fn = ctor_val.to_as_function();
				}
			}
		}
		if (super_fn == NULL && m_home_object != NULL)
		{
			// [DICE-MOD SwiffPlayer #30] A method: the prototype it was defined on knows its parent.
			as_value	ctor_val;
			as_member	own;
			if (m_home_object->m_members.get("__constructor__", &own))
			{
				super_fn = own.get_member_value().to_as_function();
			}
		}
		struct call_binding_scope
		{
			as_environment*		m_e;
			as_object_interface*	m_saved_this;
			as_as_function*		m_saved_super;
			~call_binding_scope() { m_e->m_this_object = m_saved_this; m_e->m_super_ctor = m_saved_super; }
		};
		call_binding_scope	binding_scope = { our_env, our_env->m_this_object, our_env->m_super_ctor };
		our_env->m_this_object = this_obj;
		our_env->m_super_ctor = super_fn;

		// Set up local stack frame, for parameters and locals.
		int	local_stack_top = our_env->get_local_frame_top();
		our_env->add_function_barrier();	// [DICE-MOD SwiffPlayer #41] a marked barrier - see in_function_frame()

		// [DICE-MOD SwiffPlayer #57] The register bank a call borrows is the CALLER's. The shipped
		// DefineFunction2 and DefineFunction (execute cases 142 / 155) construct every function with a
		// NULL environment (0x10094AEA pushes 0 as the ctor's env; sub_1008E870 stores it at +0x10),
		// and the dispatch (sub_10095840) runs the body in that m_env or, when it is NULL, in the
		// environment the caller passed - so every function runs in its caller's environment, on the
		// caller's registers. This port keeps the DEFINING environment for scope (the stock 2006
		// model; the open item under #55 in GAMESWF_DIVERGENCES.md) and borrows only the calling
		// chain's bank for the call. Without it every class method - defined by the root's DoInitAction
		// code - ran on the ROOT's bank: the `dice.UI` constructor (mainMenu.swf sprite 5,
		// `function UI(cancelInit)`, cancelInit in r3) is called by the load path with no argument in
		// the new clip's own environment (sub_10081870 -> sub_1008EAA0(this + 288, ...)), where r3 is
		// undefined; here it read whatever root code had left in r3, and a truthy value skipped
		// init/draw/attachChildren/arrange/setEvents - the component kept its grey authoring box
		// (mcBoundingBox) and never built its parts (the owner's login page, 2026-09-29: grey bars for
		// PREFIX, the password rows and the LOG IN button).
		as_environment*	call_bank = (fn.env ? fn.env : our_env)->register_bank();
		struct register_bank_scope
		{
			as_environment*	m_e;
			as_environment*	m_saved;
			~register_bank_scope() { m_e->m_register_env = m_saved; }
		};
		register_bank_scope	bank_scope = { our_env, our_env->m_register_env };
		our_env->m_register_env = (call_bank == our_env) ? NULL : call_bank;

		// [DICE-MOD SwiffPlayer #55] A DefineFunction2 call borrows the environment's register bank
		// instead of pushing fresh registers. The shipped dispatch (sub_10095840) copies r0..r9 aside
		// (a fixed ten, whatever the function's RegisterCount), writes each declared argument the
		// caller actually passed into its register, writes the preloads, runs the body, and copies
		// r0..r9 back. Nothing else is cleared: an argument the caller left out reads the value the
		// register already held - in practice the caller's own register. mainMenu.swf depends on it:
		// the LAN CREATE settings panel (sprite 502) runs `flush()` -> infantryOnly.setValue(x) with
		// ONE argument -> runCallback -> callFunction -> setInfantryOnly -> flush() again. In the
		// nested flush, setValue's noCallback (r3) still holds callFunction's `obj` (r3, the panel -
		// truthy), so the shipped player skips the callback and the loop ends one level down; with
		// fresh registers noCallback was undefined and the recursion overflowed the stack (the
		// owner's crash on MULTIPLAYER -> LOCAL -> CREATE, 2026-09-29). Registers r10 and up are not
		// saved, exactly as shipped: a body that writes them leaves the values behind.
		as_value	saved_registers[10];
		if (m_is_function2)
		{
			for (int r = 0; r < 10; r++)
			{
				saved_registers[r] = *(our_env->register_ptr(r));
			}
		}

		if (m_is_function2 == false)
		{
			// Conventional function.

			// Push the arguments onto the local frame.
			int	args_to_pass = imin(fn.nargs, m_args.size());
			for (int i = 0; i < args_to_pass; i++)
			{
				assert(m_args[i].m_register == 0);
				our_env->add_local(m_args[i].m_name, fn.arg(i));
			}
		}
		else
		{
			// function2: most args go in registers; any others get pushed.
			// [DICE-MOD SwiffPlayer #55] no fresh registers - the environment's bank, saved above.

			// Handle the explicit args. [DICE-MOD #55: the shipped loop writes each declared argument
			// the caller passed into ITS register, register 0 included - it never makes a named local
			// of a function2 argument. No stock movie declares one without a register.]
			int	args_to_pass = imin(fn.nargs, m_args.size());
			for (int i = 0; i < args_to_pass; i++)
			{
				*(our_env->register_ptr(m_args[i].m_register)) = fn.arg(i);
			}

			// Handle the implicit args.
			int	current_reg = 1;
			if (m_function2_flags & 0x01)
			{
				// preload 'this' into a register.  [DICE-MOD: the bound this-object, see above]
				(*(our_env->register_ptr(current_reg))).set_as_object_interface(this_obj);
				current_reg++;
			}

			if (m_function2_flags & 0x02)
			{
				// Don't put 'this' into a local var.
			}
			else
			{
				// Put 'this' in a local var.  [DICE-MOD: the bound this-object, see above]
				our_env->add_local("this", as_value(this_obj));
			}

			// Init arguments array, if it's going to be needed.
			smart_ptr<as_array_object>	arg_array;
			if ((m_function2_flags & 0x04) || ! (m_function2_flags & 0x08))
			{
				arg_array = new as_array_object;

				as_value	index_number;
				for (int i = 0; i < fn.nargs; i++)
				{
					index_number.set_int(i);
					arg_array->set_member(index_number.to_string(), fn.arg(i));
				}
			}

			if (m_function2_flags & 0x04)
			{
				// preload 'arguments' into a register.
				(*(our_env->register_ptr(current_reg))).set_as_object_interface(arg_array.get_ptr());
				current_reg++;
			}

			if (m_function2_flags & 0x08)
			{
				// Don't put 'arguments' in a local var.
			}
			else
			{
				// Put 'arguments' in a local var.
 				our_env->add_local("arguments", as_value(arg_array.get_ptr()));
			}

			if (m_function2_flags & 0x10)
			{
				// Put 'super' in a register.  [DICE-MOD: the superclass constructor bound above -
				// the shipped dispatch preloads its env slot +8216 as an AS_FUNCTION value]
				if (super_fn)
				{
					(*(our_env->register_ptr(current_reg))) = as_value(super_fn);
				}
				else
				{
					(*(our_env->register_ptr(current_reg))) = as_value();
				}
				current_reg++;
			}

			if (m_function2_flags & 0x20)
			{
				// Don't put 'super' in a local var.
			}
			else
			{
				// Put 'super' in a local var.  [DICE-MOD: see above]
				our_env->add_local("super", super_fn ? as_value(super_fn) : as_value());
			}

			if (m_function2_flags & 0x40)
			{
				// Put '_root' in a register.
				(*(our_env->register_ptr(current_reg))).set_as_object_interface(
					our_env->m_target->get_root_movie());
				current_reg++;
			}

			if (m_function2_flags & 0x80)
			{
				// Put '_parent' in a register.
				array<with_stack_entry>	dummy;
				as_value	parent = our_env->get_variable("_parent", dummy);
				(*(our_env->register_ptr(current_reg))) = parent;
				current_reg++;
			}

			if (m_function2_flags & 0x100)
			{
				// Put '_global' in a register.
				(*(our_env->register_ptr(current_reg))).set_as_object_interface(s_global.get_ptr());
				current_reg++;
			}
		}

		// Execute the actions.
		m_action_buffer->execute(our_env, m_start_pc, m_length, fn.result, m_with_stack, m_is_function2);

		// Clean up stack frame.
		our_env->set_local_frame_top(local_stack_top);

		if (m_is_function2)
		{
			// [DICE-MOD SwiffPlayer #55] r0..r9 back as they were before the call - the shipped
			// dispatch's closing copy. Registers the body wrote above r9 stay written.
			for (int r = 0; r < 10; r++)
			{
				*(our_env->register_ptr(r)) = saved_registers[r];
			}
		}
	}


	//
	// Function/method dispatch.
	//


	as_value	call_method(
		const as_value& method,
		as_environment* env,
		as_object_interface* this_ptr,
		int nargs,
		int first_arg_bottom_index)
	// first_arg_bottom_index is the stack index, from the bottom, of the first argument.
	// Subsequent arguments are at *lower* indices.  E.g. if first_arg_bottom_index = 7,
	// then arg1 is at env->bottom(7), arg2 is at env->bottom(6), etc.
	{
		as_value	val;

		as_c_function_ptr	func = method.to_c_function();
		if (func)
		{
			// It's a C function.  Call it.
			(*func)(fn_call(&val, this_ptr, env, nargs, first_arg_bottom_index));
		}
		else if (as_as_function* as_func = method.to_as_function())
		{
			// It's an ActionScript function.  Call it.
			(*as_func)(fn_call(&val, this_ptr, env, nargs, first_arg_bottom_index));
		}
		else
		{
			log_error("error in call_method(): method is not a function\n");
		}

		return val;
	}


	as_value	call_method0(
		const as_value& method,
		as_environment* env,
		as_object_interface* this_ptr)
	{
		return call_method(method, env, this_ptr, 0, env->get_top_index() + 1);
	}
		
	const char*	call_method_parsed(
		as_environment* env,
		as_object_interface* this_ptr,
		const char* method_name,
		const char* method_arg_fmt,
		va_list args)
	// Printf-like vararg interface for calling ActionScript.
	// Handy for external binding.
	{
		log_msg("FIXME(%d): %s\n", __LINE__, __FUNCTION__);

#if 0
		static const int	BUFSIZE = 1000;
		char	buffer[BUFSIZE];
		array<const char*>	tokens;

		// Brutal crap parsing.  Basically null out any
		// delimiter characters, so that the method name and
		// args sit in the buffer as null-terminated C
		// strings.  Leave an intial ' character as the first
		// char in a string argument.
		// Don't verify parens or matching quotes or anything.
		{
			strncpy(buffer, method_call, BUFSIZE);
			buffer[BUFSIZE - 1] = 0;
			char*	p = buffer;

			char	in_quote = 0;
			bool	in_arg = false;
			for (;; p++)
			{
				char	c = *p;
				if (c == 0)
				{
					// End of string.
					break;
				}
				else if (c == in_quote)
				{
					// End of quotation.
					assert(in_arg);
					*p = 0;
					in_quote = 0;
					in_arg = false;
				}
				else if (in_arg)
				{
					if (in_quote == 0)
					{
						if (c == ')' || c == '(' || c == ',' || c == ' ')
						{
							// End of arg.
							*p = 0;
							in_arg = false;
						}
					}
				}
				else
				{
					// Not in arg.  Watch for start of arg.
					assert(in_quote == 0);
					if (c == '\'' || c == '\"')
					{
						// Start of quote.
						in_quote = c;
						in_arg = true;
						*p = '\'';	// ' at the start of the arg, so later we know this is a string.
						tokens.push_back(p);
					}
					else if (c == ' ' || c == ',')
					{
						// Non-arg junk, null it out.
						*p = 0;
					}
					else
					{
						// Must be the start of a numeric arg.
						in_arg = true;
						tokens.push_back(p);
					}
				}
			}
		}
#endif // 0


		// Parse va_list args
		int	starting_index = env->get_top_index();
		const char* p = method_arg_fmt;
		for (;; p++)
		{
			char	c = *p;
			if (c == 0)
			{
				// End of args.
				break;
			}
			else if (c == '%')
			{
				p++;
				c = *p;
				// Here's an arg.
				if (c == 'd')
				{
					// Integer.
					env->push(va_arg(args, int));
				}
				else if (c == 'f')
				{
					// Double
					env->push(va_arg(args, double));
				}
				else if (c == 's')
				{
					// String
					env->push(va_arg(args, const char *));
				}
				else if (c == 'l')
				{
					p++;
					c = *p;
					if (c == 's')
					{
						// Wide string.
						env->push(va_arg(args, const wchar_t *));
					}
					else
					{
						log_error("call_method_parsed('%s','%s') -- invalid fmt '%%l%c'\n",
							  method_name,
							  method_arg_fmt,
							  c);
					}
				}
				else
				{
					// Invalid fmt, warn.
					log_error("call_method_parsed('%s','%s') -- invalid fmt '%%%c'\n",
						  method_name,
						  method_arg_fmt,
						  c);
				}
			}
			else
			{
				// Ignore whitespace and commas.
				if (c == ' ' || c == '\t' || c == ',')
				{
					// OK
				}
				else
				{
					// Invalid arg; warn.
					log_error("call_method_parsed('%s','%s') -- invalid char '%c'\n",
						  method_name,
						  method_arg_fmt,
						  c);
				}
			}
		}

		array<with_stack_entry>	dummy_with_stack;
		as_value	method = env->get_variable(method_name, dummy_with_stack);

		// check method

		// Reverse the order of pushed args
		int	nargs = env->get_top_index() - starting_index;
		for (int i = 0; i < (nargs >> 1); i++)
		{
			int	i0 = starting_index + 1 + i;
			int	i1 = starting_index + nargs - i;
			assert(i0 < i1);

			swap(&(env->bottom(i0)), &(env->bottom(i1)));
		}

		// Do the call.
		as_value	result = call_method(method, env, this_ptr, nargs, env->get_top_index());
		env->drop(nargs);

		// Return pointer to static string for return value.
		static tu_string	s_retval;
		s_retval = result.to_tu_string();
		return s_retval.c_str();
	}


	//
	// sound object
	//

	struct sound_as_object : public as_object
	{
		tu_string sound;
		int sound_id;
	};

	void	movie_load()
	{
		IF_VERBOSE_ACTION(log_msg("-- start movie \n"));
	}

	void	sound_start(const fn_call& fn)
	{
		IF_VERBOSE_ACTION(log_msg("-- start sound \n"));
		sound_handler* s = get_sound_handler();
		if (s != NULL)
		{
			sound_as_object*	so = (sound_as_object*) (as_object*) fn.this_ptr;
			assert(so);
			s->play_sound(so->sound_id, 0);
		}
	}


	void	sound_stop(const fn_call& fn)
	{
		IF_VERBOSE_ACTION(log_msg("-- stop sound \n"));
		sound_handler* s = get_sound_handler();
		if (s != NULL)
		{
			sound_as_object*	so = (sound_as_object*) (as_object*) fn.this_ptr;
			assert(so);
			s->stop_sound(so->sound_id);
		}
	}

	void	sound_attach(const fn_call& fn)
	{
		IF_VERBOSE_ACTION(log_msg("-- attach sound \n"));
		if (fn.nargs < 1)
		{
			log_error("attach sound needs one argument\n");
			return;
		}

		sound_as_object*	so = (sound_as_object*) (as_object*) fn.this_ptr;
		assert(so);

		so->sound = fn.arg(0).to_tu_string();

		// check the import.
		movie_definition_sub*	def = (movie_definition_sub*)
			fn.env->get_target()->get_root_movie()->get_movie_definition();
		assert(def);
		smart_ptr<resource> res = def->get_exported_resource(so->sound);
		if (res == NULL)
		{
			log_error("import error: resource '%s' is not exported\n", so->sound.c_str());
			return;
		}

		int si = 0;
		sound_sample_impl* ss = (sound_sample_impl*) res->cast_to_sound_sample();

		if (ss != NULL)
		{
			si = ss->m_sound_handler_id;
		}
		else
		{
			log_error("sound sample is NULL\n");
			return;
		}

		// sanity check
		assert(si >= 0 && si < 1000);
		so->sound_id = si;
	}

	//
	// Built-in objects
	//


	//
	// math object
	//


#if 0
	// One-argument simple functions.
	#define MATH_WRAP_FUNC1(funcname)							\
	void	math_##funcname(as_value* result, as_object_interface* this_ptr,		\
				as_environment* env, int nargs, int first_arg_bottom_index)	\
	{											\
		double	arg = env->bottom(first_arg_bottom_index).to_number();			\
		result->set_double(funcname(arg));						\
	}
#else
	// One-argument simple functions.
	#define MATH_WRAP_FUNC1(funcname)							\
	void	math_##funcname(const fn_call& fn)						\
	{											\
		double	arg = fn.arg(0).to_number();						\
		fn.result->set_double(funcname(arg));						\
	}
#endif

	MATH_WRAP_FUNC1(fabs);
	MATH_WRAP_FUNC1(acos);
	MATH_WRAP_FUNC1(asin);
	MATH_WRAP_FUNC1(atan);
	MATH_WRAP_FUNC1(ceil);
	MATH_WRAP_FUNC1(cos);
	MATH_WRAP_FUNC1(exp);
	MATH_WRAP_FUNC1(floor);
	MATH_WRAP_FUNC1(log);
	MATH_WRAP_FUNC1(sin);
	MATH_WRAP_FUNC1(sqrt);
	MATH_WRAP_FUNC1(tan);

#if 0
	// Two-argument functions.
	#define MATH_WRAP_FUNC2_EXP(funcname, expr)										\
	void	math_##funcname(as_value* result, as_object_interface* this_ptr, as_environment* env, int nargs, int first_arg_bottom_index)	\
	{															\
		double	arg0 = env->bottom(first_arg_bottom_index).to_number();							\
		double	arg1 = env->bottom(first_arg_bottom_index - 1).to_number();						\
		result->set_double(expr);											\
	}
#else
	// Two-argument functions.
	#define MATH_WRAP_FUNC2_EXP(funcname, expr)										\
	void	math_##funcname(const fn_call& fn)										\
	{															\
		double	arg0 = fn.arg(0).to_number();										\
		double	arg1 = fn.arg(1).to_number();										\
		fn.result->set_double(expr);											\
	}
#endif
	MATH_WRAP_FUNC2_EXP(atan2, (atan2(arg0, arg1)));
	MATH_WRAP_FUNC2_EXP(max, (arg0 > arg1 ? arg0 : arg1));
	MATH_WRAP_FUNC2_EXP(min, (arg0 < arg1 ? arg0 : arg1));
	MATH_WRAP_FUNC2_EXP(pow, (pow(arg0, arg1)));

	// A couple of oddballs.
	void	math_random(const fn_call& fn)
	{
		// Random number between 0 and 1.
		fn.result->set_double(tu_random::next_random() / double(Uint32(0x0FFFFFFFF)));
	}
	void	math_round(const fn_call& fn)
	{
		// round argument to nearest int.
		double	arg0 = fn.arg(0).to_number();
		fn.result->set_double(floor(arg0 + 0.5));
	}
	
	void math_init()
	{
		// Create built-in math object.
		as_object*	math_obj = new as_object;

		// constant
		math_obj->set_member("e", 2.7182818284590452354);
		math_obj->set_member("ln2", 0.69314718055994530942);
		math_obj->set_member("log2e", 1.4426950408889634074);
		math_obj->set_member("ln10", 2.30258509299404568402);
		math_obj->set_member("log10e", 0.43429448190325182765);
		math_obj->set_member("pi", 3.14159265358979323846);
		math_obj->set_member("sqrt1_2", 0.7071067811865475244);
		math_obj->set_member("sqrt2", 1.4142135623730950488);

		// math methods
		math_obj->set_member("abs", &math_fabs);
		math_obj->set_member("acos", &math_acos);
		math_obj->set_member("asin", &math_asin);
		math_obj->set_member("atan", &math_atan);
		math_obj->set_member("ceil", &math_ceil);
		math_obj->set_member("cos", &math_cos);
		math_obj->set_member("exp", &math_exp);
		math_obj->set_member("floor", &math_floor);
		math_obj->set_member("log", &math_log);
		math_obj->set_member("random", &math_random);
		math_obj->set_member("round", &math_round);
		math_obj->set_member("sin", &math_sin);
		math_obj->set_member("sqrt", &math_sqrt);
		math_obj->set_member("tan", &math_tan);

		math_obj->set_member("atan2", &math_atan2);
		math_obj->set_member("max", &math_max);
		math_obj->set_member("min", &math_min);
		math_obj->set_member("pow", &math_pow);

		s_global->set_member("math", math_obj);
	}
		
	void event_test(const fn_call& fn)
	{
		log_msg("FIXME: %s\n", __FUNCTION__);
	}
	
	//
	// key object
	//


	struct key_as_object : public as_object
	{
		Uint8	m_keymap[key::KEYCOUNT / 8 + 1];	// bit-array
		array<weak_ptr<as_object_interface> >	m_listeners;
		int	m_last_key_pressed;

		key_as_object()
			:
			m_last_key_pressed(0)
		{
			memset(m_keymap, 0, sizeof(m_keymap));
		}

		bool	is_key_down(int code)
		{
			if (code < 0 || code >= key::KEYCOUNT) return false;

			int	byte_index = code >> 3;
			int	bit_index = code - (byte_index << 3);
			int	mask = 1 << bit_index;

			assert(byte_index >= 0 && byte_index < int(sizeof(m_keymap)/sizeof(m_keymap[0])));

			if (m_keymap[byte_index] & mask)
			{
				return true;
			}
			else
			{
				return false;
			}
		}

		void	set_key_down(int code)
		{
			if (code < 0 || code >= key::KEYCOUNT) return;

			m_last_key_pressed = code;

			int	byte_index = code >> 3;
			int	bit_index = code - (byte_index << 3);
			int	mask = 1 << bit_index;

			assert(byte_index >= 0 && byte_index < int(sizeof(m_keymap)/sizeof(m_keymap[0])));

			m_keymap[byte_index] |= mask;

			// Notify listeners.
			int i;
			int n = m_listeners.size();
			for (i = 0; i < n; i++)
			{
				smart_ptr<as_object_interface>	listener = m_listeners[i];
				as_value	method;
				if (listener != NULL
				    && listener->get_member(event_id(event_id::KEY_DOWN).get_function_name(), &method))
				{
					call_method(method, NULL /* or root? */, listener.get_ptr(), 0, 0);
				}
			}
		}

		void	set_key_up(int code)
		{
			if (code < 0 || code >= key::KEYCOUNT) return;

			int	byte_index = code >> 3;
			int	bit_index = code - (byte_index << 3);
			int	mask = 1 << bit_index;

			assert(byte_index >= 0 && byte_index < int(sizeof(m_keymap)/sizeof(m_keymap[0])));

			m_keymap[byte_index] &= ~mask;

			// Notify listeners.
			for (int i = 0, n = m_listeners.size(); i < n; i++)
			{
				smart_ptr<as_object_interface>	listener = m_listeners[i];

				as_value	method;
				if (listener != NULL
				    && listener->get_member(event_id(event_id::KEY_UP).get_function_name(), &method))
				{
					call_method(method, NULL /* or root? */, listener.get_ptr(), 0, 0);
				}
			}
		}

		void	cleanup_listeners()
		// Remove dead entries in the listeners list.  (Since
		// we use weak_ptr's, listeners can disappear without
		// notice.)
		{
			for (int i = m_listeners.size() - 1; i >= 0; i--)
			{
				if (m_listeners[i] == NULL)
				{
					m_listeners.remove(i);
				}
			}
		}

		void	add_listener(as_object_interface* listener)
		{
			cleanup_listeners();

			for (int i = 0, n = m_listeners.size(); i < n; i++)
			{
				if (m_listeners[i] == listener)
				{
					// Already in the list.
					return;
				}
			}

			m_listeners.push_back(listener);
		}

		void	remove_listener(as_object_interface* listener)
		{
			cleanup_listeners();

			for (int i = m_listeners.size() - 1; i >= 0; i--)
			{
				if (m_listeners[i] == listener)
				{
					m_listeners.remove(i);
				}
			}
		}

		int	get_last_key_pressed() const { return m_last_key_pressed; }
	};


	void	key_add_listener(const fn_call& fn)
	// Add a listener (first arg is object reference) to our list.
	// Listeners will have "onKeyDown" and "onKeyUp" methods
	// called on them when a key changes state.
	{
		if (fn.nargs < 1)
		{
			log_error("key_add_listener needs one argument (the listener object)\n");
			return;
		}

		as_object_interface*	listener = fn.arg(0).to_object();
		if (listener == NULL)
		{
			log_error("key_add_listener passed a NULL object; ignored\n");
			return;
		}

		key_as_object*	ko = (key_as_object*) (as_object*) fn.this_ptr;
		assert(ko);

		ko->add_listener(listener);
	}

	void	key_get_ascii(const fn_call& fn)
	// Return the ascii value of the last key pressed.
	{
		key_as_object*	ko = (key_as_object*) (as_object*) fn.this_ptr;
		assert(ko);

		fn.result->set_undefined();

		int	code = ko->get_last_key_pressed();
		if (code > 0)
		{
			// @@ Crude for now; just jamming the key code in a string, as a character.
			// Need to apply shift/capslock/numlock, etc...
			char	buf[2];
			buf[0] = (char) code;
			buf[1] = 0;

			fn.result->set_string(buf);
		}
	}

	void	key_get_code(const fn_call& fn)
	// Returns the keycode of the last key pressed.
	{
		key_as_object*	ko = (key_as_object*) (as_object*) fn.this_ptr;
		assert(ko);

		fn.result->set_int(ko->get_last_key_pressed());
	}

	void	key_is_down(const fn_call& fn)
	// Return true if the specified (first arg keycode) key is pressed.
	{
		if (fn.nargs < 1)
		{
			log_error("key_is_down needs one argument (the key code)\n");
			return;
		}

		int	code = (int) fn.arg(0).to_number();

		key_as_object*	ko = (key_as_object*) (as_object*) fn.this_ptr;
		assert(ko);

		fn.result->set_bool(ko->is_key_down(code));
	}

	void	key_is_toggled(const fn_call& fn)
	// Given the keycode of NUM_LOCK or CAPSLOCK, returns true if
	// the associated state is on.
	{
		// @@ TODO
		fn.result->set_bool(false);
	}

	void	key_remove_listener(const fn_call& fn)
	// Remove a previously-added listener.
	{
		if (fn.nargs < 1)
		{
			log_error("key_remove_listener needs one argument (the listener object)\n");
			return;
		}

		as_object_interface*	listener = fn.arg(0).to_object();
		if (listener == NULL)
		{
			log_error("key_remove_listener passed a NULL object; ignored\n");
			return;
		}

		key_as_object*	ko = (key_as_object*) (as_object*) fn.this_ptr;
		assert(ko);

		ko->remove_listener(listener);
	}


	void key_init()
	{
		// Create built-in key object.
		as_object*	key_obj = new key_as_object;

		// constants
#define KEY_CONST(k) key_obj->set_member(#k, key::k)
		KEY_CONST(BACKSPACE);
		KEY_CONST(CAPSLOCK);
		KEY_CONST(CONTROL);
		KEY_CONST(DELETEKEY);
		KEY_CONST(DOWN);
		KEY_CONST(END);
		KEY_CONST(ENTER);
		KEY_CONST(ESCAPE);
		KEY_CONST(HOME);
		KEY_CONST(INSERT);
		KEY_CONST(LEFT);
		KEY_CONST(PGDN);
		KEY_CONST(PGUP);
		KEY_CONST(RIGHT);
		KEY_CONST(SHIFT);
		KEY_CONST(SPACE);
		KEY_CONST(TAB);
		KEY_CONST(UP);

		// methods
		key_obj->set_member("addListener", &key_add_listener);
		key_obj->set_member("getAscii", &key_get_ascii);
		key_obj->set_member("getCode", &key_get_code);
		key_obj->set_member("isDown", &key_is_down);
		key_obj->set_member("isToggled", &key_is_toggled);
		key_obj->set_member("removeListener", &key_remove_listener);

		s_global->set_member("Key", key_obj);
	}


	void	notify_key_event(key::code k, bool down)
	// External interface for the host to report key events.
	{
		action_init();	// @@ put this in some global init somewhere else...

		static tu_string	key_obj_name("Key");

		as_value	kval;
		s_global->get_member(key_obj_name, &kval);
		if (kval.get_type() == as_value::OBJECT)
		{
			key_as_object*	ko = (key_as_object*) kval.to_object();
			assert(ko);

			if (down) ko->set_key_down(k);
			else ko->set_key_up(k);
		}
		else
		{
			log_error("gameswf::notify_key_event(): no Key built-in\n");
		}
	}
	

	//
	// global init
	//


	void	as_global_trace(const fn_call& fn)
	{
		assert(fn.nargs >= 1);

		// Special case for objects: try the toString() method.
		if (fn.arg(0).get_type() == as_value::OBJECT)
		{
			as_object_interface* obj = fn.arg(0).to_object();
			assert(obj);

			as_value method;
			if (obj->get_member("toString", &method)
			    && method.is_function())
			{
				as_value result = call_method0(method, fn.env, obj);
				log_msg("%s\n", result.to_string());

				return;
			}
		}

		// Log our argument.
		//
		// @@ what if we get extra args?
		//
		// @@ Array gets special treatment.
		const char* arg0 = fn.arg(0).to_string();
		log_msg("%s\n", arg0);
	}


	void	as_global_sound_ctor(const fn_call& fn)
	// Constructor for ActionScript class Sound.
	{
		smart_ptr<as_object>	sound_obj(new sound_as_object);

		// methods
		sound_obj->set_member("attachSound", &sound_attach);
		sound_obj->set_member("start", &sound_start);
		sound_obj->set_member("stop", &sound_stop);

		fn.result->set_as_object_interface(sound_obj.get_ptr());
	}


	void	as_global_object_ctor(const fn_call& fn)
	// Constructor for ActionScript class Object.
	{
		fn.result->set_as_object_interface(new as_object);
	}

	void	as_global_array_ctor(const fn_call& fn)
	// Constructor for ActionScript class Array.
	{
		smart_ptr<as_array_object>	ao = new as_array_object;

		if (fn.nargs == 0)
		{
			// Empty array.
		}
		else if (fn.nargs == 1
			 && fn.arg(0).get_type() == as_value::NUMBER)
		{
			// Create an empty array with the given number of undefined elements.
			// [DICE-MOD: the length is live now]
			ao->m_length = (int) fn.arg(0).to_number();
			if (ao->m_length < 0) ao->m_length = 0;
		}
		else
		{
			// Use the arguments as initializers.
			as_value	index_number;
			for (int i = 0; i < fn.nargs; i++)
			{
				index_number.set_int(i);
				ao->set_member(index_number.to_string(), fn.arg(i));
			}

			// @@ TODO set length property
		}

		fn.result->set_as_object_interface(ao.get_ptr());
	}


	void	as_global_assetpropflags(const fn_call& fn)
	// ASSetPropFlags function
	{
		const int version = fn.env->get_target()->get_movie_definition()->get_version();

		// Check the arguments
		assert(fn.nargs == 3 || fn.nargs == 4);
		assert((version == 5) ? (fn.nargs == 3) : true);

		// object
		as_object_interface* const obj = fn.arg(0).to_object();
		assert(obj != NULL);

		// list of child names
		as_object_interface* props = fn.arg(1).to_object();
		if (props == NULL) {
			// tulrich: this fires in test_ASSetPropFlags -- is it correct?
			assert(fn.arg(1).get_type() == as_value::NULLTYPE);
		}

		// a number which represents three bitwise flags which
		// are used to determine whether the list of child names should be hidden,
		// un-hidden, protected from over-write, un-protected from over-write,
		// protected from deletion and un-protected from deletion
		int set_true = int(fn.arg(2).to_number()) & as_prop_flags::as_prop_flags_mask;

		// Is another integer bitmask that works like set_true,
		// except it sets the attributes to false. The
		// set_false bitmask is applied before set_true is applied

		// ASSetPropFlags was exposed in Flash 5, however the fourth argument 'set_false'
		// was not required as it always defaulted to the value '~0'. 
		int set_false = (fn.nargs == 3 ? 
				 (version == 5 ? ~0 : 0) : int(fn.arg(3).to_number()))
			& as_prop_flags::as_prop_flags_mask;

		// Evan: it seems that if set_true == 0 and set_false == 0, this function
		// acts as if the parameters where (object, null, 0x1, 0) ...
		if (set_false == 0 && set_true == 0)
		{
			props = NULL;
			set_false = 0;
			set_true = 0x1;
		}

		if (props == NULL)
		{
			// Take all the members of the object

			as_object* object = (as_object*) obj;

			stringi_hash<as_member>::const_iterator it = object->m_members.begin();
			while (it != object->m_members.end())
			{
				as_member member = it.get_value();

				as_prop_flags f = member.get_member_flags();
				const int oldflags = f.get_flags();
				const int newflags = f.set_flags(set_true, set_false);
				member.set_member_flags(f);

				object->m_members.set(it.get_key(), member);

				++it;
			}

			// [DICE-MOD SwiffPlayer #29: STOCK 2006 BUG REMOVED. With a null property list the stock
			// body went on to walk object->m_prototype and copied every member of the PROTOTYPE down
			// into the object, overwriting the object's own members of the same name. Every DICE class
			// ends its init action with `ASSetPropFlags(Class.prototype, null, 1)`, so each subclass
			// prototype had its own init/draw/createChildren/arrange replaced by the parent's - the
			// whole UI ran UIObject's empty draw and never created a child clip (harness X, 2026-09-02:
			// profilesList's prototype 038C4E44 stored draw=04257424, the binding read 04250F24).
			// Flash's ASSetPropFlags(obj, null, flags) changes the flags of obj's OWN properties only.]
		} else {
			as_object* object = (as_object*) obj;
			as_object* object_props = (as_object*) props;

			stringi_hash<as_member>::iterator it = object_props->m_members.begin();
			while(it != object_props->m_members.end())
			{
				const tu_stringi key = (it.get_value()).get_member_value().to_string();
				stringi_hash<as_member>::iterator it2 = object->m_members.find(key);

				if (it2 != object->m_members.end())
				{
					as_member member = it2.get_value();

					as_prop_flags f = member.get_member_flags();
					const int oldflags = f.get_flags();
					const int newflags = f.set_flags(set_true, set_false);
					member.set_member_flags(f);

					object->m_members.set((it.get_value()).get_member_value().to_string(), member);
				}

				++it;
			}
		}
	}


	// =============================================================================================
	// [DICE-MOD SwiffPlayer: the AS2 `Color` class - absent from stock 2006 gameswf, present in the
	// shipped DICE build, and called by BF2's menu (`new Color(mc).setRGB(rgb)`). On stock gameswf
	// that logged "can't create object with unknown class 'Color'" and the tinted clips kept their
	// authored colours: retail 2026-09-02, the login panel drew its 0x844D00 orange where the golden
	// run (shipped player, same renderer) drew 0x464646 grey.
	//
	// Recovered from SwiffPlayer.dll (2).i64:
	//   * the constructor c_function (sub_1008B280): a 44-byte as_object whose ctor (sub_100A96A0)
	//     registers getRGB = sub_100A9570 and setRGB = gameswf_color_set_rgb as PER-INSTANCE
	//     members; +24 rgb = 0; +28 target = the first argument iff it is an OBJECT, else null;
	//   * setRGB (gameswf_color_set_rgb): exactly one argument, else log "Color:setRGB needs one
	//     argument"; with a target: rgb = to_number(arg0); copy the target character's cxform
	//     (character +124 in the 2004 layout); r/g/b mult = 0, add = the rgb bytes; the alpha pair
	//     is copied through untouched; push it through the character's set_cxform;
	//   * getRGB (sub_100A9570): the stored rgb as a NUMBER.
	// No setTransform/getTransform - the shipped ctor registers only those two. The shipped bodies
	// take `this` as a Color without checking; ours refuses a foreign `this` via cast_to_as_object.]
	// =============================================================================================
	struct as_color : public as_object
	{
		int				m_rgb;		// +24 in the shipped object
		smart_ptr<as_object_interface>	m_target;	// +28 (a raw pointer in the shipped object)

		as_color() : m_rgb(0) {}
	};

	void	color_set_rgb(const fn_call& fn)
	{
		if (fn.nargs != 1)
		{
			log_error("Color:setRGB needs one argument\n");
			return;
		}
		as_color*	c = fn.this_ptr ? static_cast<as_color*>(fn.this_ptr->cast_to_as_object()) : NULL;
		if (c == NULL || c->m_target == NULL)
		{
			return;
		}
		c->m_rgb = (int) fn.arg(0).to_number();

		movie*		mv = c->m_target->to_movie();
		character*	ch = mv ? mv->cast_to_character() : NULL;
		if (ch == NULL)
		{
			return;
		}
		cxform	cx = ch->get_cxform();
		cx.m_[0][0] = 0.0f;	cx.m_[0][1] = (float) ((c->m_rgb >> 16) & 0xFF);
		cx.m_[1][0] = 0.0f;	cx.m_[1][1] = (float) ((c->m_rgb >> 8) & 0xFF);
		cx.m_[2][0] = 0.0f;	cx.m_[2][1] = (float) (c->m_rgb & 0xFF);
		// cx.m_[3][*] (alpha) stays as copied - the shipped body leaves it alone.
		ch->set_cxform(cx);
	}

	void	color_get_rgb(const fn_call& fn)
	{
		as_color*	c = fn.this_ptr ? static_cast<as_color*>(fn.this_ptr->cast_to_as_object()) : NULL;
		fn.result->set_double(c ? (double) c->m_rgb : 0.0);
	}

	void	color_ctor(const fn_call& fn)
	{
		as_color*	c = new as_color;
		c->set_member("getRGB", as_value(color_get_rgb));
		c->set_member("setRGB", as_value(color_set_rgb));
		if (fn.nargs > 0 && fn.arg(0).get_type() == as_value::OBJECT)
		{
			c->m_target = fn.arg(0).to_object();
		}
		fn.result->set_as_object_interface(c);
	}


	// =============================================================================================
	// [DICE-MOD SwiffPlayer: Object.registerClass(symbolName, Class) - the AS2 idiom that binds a
	// library symbol to a component class, absent from stock 2006 (which only knows methods on the
	// String constructor and logs "method call on unknown c function" for everything else - BF2's
	// menu makes 18 such calls before its first frame, one per UI component class).
	//
	// Recovered from SwiffPlayer.dll (2).i64: DICE's `Object` is a class object whose ctor
	// (sub_1009A190) registers registerClass = sub_100997B0; that body needs exactly two arguments
	// ("Object::registerClass need two arguments"), converts arg0 to a string and arg1 to a
	// function, asks the root movie's definition for get_exported_resource(name), and stores the
	// function in the resulting character_def (+12). The bound class is applied when a sprite of
	// that definition loads - see sprite_instance::bind_registered_class (gameswf_impl.cpp).
	// Here `Object` stays the stock C function; the CallMethod handler routes the one method the
	// shipped object carries to this body. See GAMESWF_DIVERGENCES.md.]
	// =============================================================================================
	void	object_register_class(const fn_call& fn)
	{
		if (fn.nargs != 2)
		{
			log_error("Object::registerClass need two arguments\n");
			return;
		}
		const tu_string	symbol = fn.arg(0).to_tu_string();
		as_as_function*	cls = fn.arg(1).to_as_function();

		movie*	target = fn.env->get_target();
		movie*	root = target ? target->get_root_movie() : NULL;
		movie_definition*	def = root ? root->get_movie_definition() : NULL;
		if (def == NULL || cls == NULL)
		{
			fn.result->set_bool(false);
			return;
		}
		// Every movie_definition in this tree is a movie_definition_sub (movie_def_impl,
		// sprite_definition), which is where the export table lives.
		smart_ptr<resource>	res = static_cast<movie_definition_sub*>(def)->get_exported_resource(symbol);
		character_def*	cdef = res.get_ptr() ? res->cast_to_character_def() : NULL;
		if (cdef == NULL)
		{
			log_error("Object::registerClass: no exported character '%s'\n", symbol.c_str());
			fn.result->set_bool(false);
			return;
		}
		cls->add_ref();
		if (cdef->m_registered_class)
		{
			cdef->m_registered_class->drop_ref();
		}
		cdef->m_registered_class = cls;
		fn.result->set_bool(true);
	}

	// [DICE-MOD SwiffPlayer: the shipped action_init (sub_10090FF0) registers "MovieClip" as a
	// no-op C function (nullsub_10). Its only job is to exist: `class X extends MovieClip` then
	// resolves a superclass that is not a script function, and the extends handler leaves the
	// subclass prototype's __proto__ empty - exactly what the shipped handler does with it.]
	void	as_global_movieclip_ctor(const fn_call& fn)
	{
		(void) fn;
	}


	void	action_init()
	// Create/hook built-ins.
	{
		if (s_inited == false)
		{
			s_inited = true;

			// @@ s_global should really be a
			// client-visible player object, which
			// contains one or more actual movie
			// instances.  We're currently just hacking it
			// in as an app-global mutable object :(
			assert(s_global == NULL);
			s_global = new as_object;
			s_global->set_member("trace", as_value(as_global_trace));
			s_global->set_member("Object", as_value(as_global_object_ctor));
			s_global->set_member("Sound", as_value(as_global_sound_ctor));
			s_global->set_member("Array", as_value(as_global_array_ctor));

			s_global->set_member("TextFormat", as_value(textformat_new));
#ifdef HAVE_LIBXML
			s_global->set_member("XML", as_value(xml_new));
			//s_global->set_member("XML", as_value(xmlsocket_xml_new));
			s_global->set_member("XMLSocket", as_value(xmlsocket_new));
#endif // HAVE_LIBXML
			s_global->set_member("MovieClipLoader", as_value(moviecliploader_new));
			s_global->set_member("String", as_value(string_ctor));
			s_global->set_member("Number", as_value(as_global_number_ctor));     // [DICE-MOD #40] sub_1008C520
			s_global->set_member("Boolean", as_value(as_global_boolean_ctor));   // [DICE-MOD #40] sub_1008C4D0

			// ASSetPropFlags
			s_global->set_member("ASSetPropFlags", as_global_assetpropflags);

			// [DICE-MOD SwiffPlayer: register setInterval/clearInterval in _global. gameswf 2006
			// IMPLEMENTS both (gameswf_timers.cpp timer_setinterval / timer_clearinterval) but
			// action_init never publishes them, so every script call reports "'setInterval' is not a
			// function" and the movie's timed callbacks never run. menu.swf calls setInterval.
			// DICE's build exposes the same pair (as_global_setinterval / ...clearinterval in the
			// shipped image). See GAMESWF_DIVERGENCES.md.]
			s_global->set_member("setInterval", as_value(timer_setinterval));
			s_global->set_member("clearInterval", as_value(timer_clearinterval));

			// [DICE-MOD SwiffPlayer: the Color class (color_ctor above). The shipped action_init
			// (sub_10090FF0) registers "Color" alongside String/Object/XML/Array/Boolean/Number.]
			s_global->set_member("Color", as_value(color_ctor));
			s_global->set_member("MovieClip", as_value(as_global_movieclip_ctor));

			math_init();
			key_init();
		}
	}


	// =============================================================================================
	// [DICE-MOD SwiffPlayer: AS2 getter/setter properties - `Object.addProperty(name, get, set)`.
	//
	// Stock 2006 gameswf has no property support; menu.swf calls addProperty 36 times in its onLoad,
	// and DICE's own AS object class registers exactly this one method. A name registered here
	// SHADOWS a plain member of the same name: reading it calls the getter, writing it calls the
	// setter. See gameswf_action.h and GAMESWF_DIVERGENCES.md.
	//
	// The getter/setter are invoked through call_method, which needs an as_environment to carry the
	// arguments; property access has none of its own, so a file-scope scratch environment is used.
	// It is only ever entered one call deep from here, and each use leaves it as it found it.
	// =============================================================================================
	static as_environment	s_property_env;

	void	as_object::add_property(const tu_stringi& name, const as_value& getter, const as_value& setter)
	{
		m_getters.set(name, getter);
		m_setters.set(name, setter);
	}

	static bool	is_callable(const as_value& v)
	{
		return v.get_type() == as_value::C_FUNCTION || v.get_type() == as_value::AS_FUNCTION;
	}

	// [DICE-MOD SwiffPlayer #28] Walk this object and its prototype chain for a getter / setter.
	bool	as_object::find_getter(const tu_stringi& name, as_value* getter, int depth)
	{
		if (m_getters.get(name, getter) && is_callable(*getter))
		{
			return true;
		}
		if (depth < 32 && m_prototype != NULL)
		{
			as_object*	proto = m_prototype->cast_to_as_object();
			if (proto)
			{
				return proto->find_getter(name, getter, depth + 1);
			}
		}
		return false;
	}

	bool	as_object::find_setter(const tu_stringi& name, as_value* setter, int depth)
	{
		if (m_setters.get(name, setter) && is_callable(*setter))
		{
			return true;
		}
		if (depth < 32 && m_prototype != NULL)
		{
			as_object*	proto = m_prototype->cast_to_as_object();
			if (proto)
			{
				return proto->find_setter(name, setter, depth + 1);
			}
		}
		return false;
	}

	// [DICE-MOD SwiffPlayer #30] The first object a function is stored on is its home (see the
	// m_home_object comment in the header).
	void	as_object::note_function_home(const as_value& fn)
	{
		as_as_function*	f = fn.to_as_function();
		if (f && f->m_home_object == NULL)
		{
			f->m_home_object = this;
		}
	}

	as_value	as_object::invoke_getter(const as_value& getter, as_object_interface* self)
	{
		return call_method0(getter, &s_property_env, self);
	}

	void	as_object::invoke_setter(const as_value& setter, as_object_interface* self, const as_value& val)
	{
		// One argument, passed the way fn_call::arg() reads it: bottom(first_arg_bottom_index - n).
		s_property_env.push(val);
		call_method(setter, &s_property_env, self, 1, s_property_env.get_top_index());
		s_property_env.drop(1);
	}

	bool	as_object::get_property(const tu_stringi& name, as_value* val)
	{
		as_value	getter;
		if (find_getter(name, &getter) == false)
		{
			return false;
		}
		*val = invoke_getter(getter, this);
		return true;
	}

	bool	as_object::set_property(const tu_stringi& name, const as_value& val)
	{
		as_value	setter;
		if (find_setter(name, &setter) == false)
		{
			return false;
		}
		invoke_setter(setter, this, val);
		return true;
	}

	// The builtin itself. `this_ptr` is an as_object_interface* that need not be an as_object (a
	// sprite_instance is not), hence the virtual cast_to_as_object rather than a static_cast.
	void	as_object_addproperty(const fn_call& fn)
	{
		if (fn.nargs < 3 || fn.this_ptr == NULL)
		{
			log_error("error: addProperty needs three arguments\n");
			fn.result->set_bool(false);
			return;
		}
		as_object*	obj = fn.this_ptr->cast_to_as_object();
		if (obj == NULL)
		{
			log_error("error: addProperty called on a non-object\n");
			fn.result->set_bool(false);
			return;
		}
		obj->add_property(fn.arg(0).to_tu_string().to_tu_stringi(), fn.arg(1), fn.arg(2));
		fn.result->set_bool(true);
	}
	// [/DICE-MOD]

	// [DICE-MOD SwiffPlayer: expose _global for the AS-bridge graph build, evidence loadMovie
	// sub_1008E4D0 fetches _global before advance. Stock gameswf keeps s_global file-scope with no
	// accessor and inits it lazily; DICE replaced the whole mechanism with a module hash and forced
	// init up-front. This is the minimal stock-gameswf equivalent: force action_init (so s_global
	// exists before the first advance) and hand back the object so the wrapper can hang dice.* off it.
	// Indexed in Code/BF2/External/gameswf/GAMESWF_DIVERGENCES.md.]
	as_object*	get_global_object()
	{
		action_init();
		return s_global.get_ptr();
	}
	// [/DICE-MOD]


	void	action_clear()
	{
		if (s_inited)
		{
			s_inited = false;

			s_global->clear();
			s_global = NULL;
		}
	}


	//
	// properties by number
	//

	static const tu_string	s_property_names[] =
	{
		tu_string("_x"),
		tu_string("_y"),
		tu_string("_xscale"),
		tu_string("_yscale"),
		tu_string("_currentframe"),
		tu_string("_totalframes"),
		tu_string("_alpha"),
		tu_string("_visible"),
		tu_string("_width"),
		tu_string("_height"),
		tu_string("_rotation"),
		tu_string("_target"),
		tu_string("_framesloaded"),
		tu_string("_name"),
		tu_string("_droptarget"),
		tu_string("_url"),
		tu_string("_highquality"),
		tu_string("_focusrect"),
		tu_string("_soundbuftime"),
		tu_string("@@ mystery quality member"),
		tu_string("_xmouse"),
		tu_string("_ymouse"),
	};


	static as_value	get_property(as_object_interface* obj, int prop_number)
	{
		as_value	val;
		if (prop_number >= 0 && prop_number < int(sizeof(s_property_names)/sizeof(s_property_names[0])))
		{
			obj->get_member(s_property_names[prop_number], &val);
		}
		else
		{
			log_error("error: invalid property query, property number %d\n", prop_number);
		}
		return val;
	}

	static void	set_property(as_object_interface* obj, int prop_number, const as_value& val)
	{
		if (prop_number >= 0 && prop_number < int(sizeof(s_property_names)/sizeof(s_property_names[0])))
		{
			obj->set_member(s_property_names[prop_number], val);
		}
		else
		{
			log_error("error: invalid set_property, property number %d\n", prop_number);
		}
	}


	//
	// do_action
	//


	// Thin wrapper around action_buffer.
	struct do_action : public execute_tag
	{
		action_buffer	m_buf;

		void	read(stream* in)
		{
			m_buf.read(in);
		}

		virtual void	execute(movie* m)
		{
			m->add_action_buffer(&m_buf);
		}

		// Don't override because actions should not be replayed when seeking the movie.
		//void	execute_state(movie* m) {}

		virtual bool	is_action_tag() const
		// Tell the caller that we are an action tag.
		{
			return true;
		}
	};

	void	do_action_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		IF_VERBOSE_PARSE(log_msg("tag %d: do_action_loader\n", tag_type));

		IF_VERBOSE_ACTION(log_msg("-- actions in frame %d\n", m->get_loading_frame()));

		assert(in);
		assert(tag_type == 12);
		assert(m);
		
		do_action*	da = new do_action;
		da->read(in);

		m->add_execute_tag(da);
	}

	
	//
	// do_init_action
	//


	void	do_init_action_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 59);

		int	sprite_character_id = in->read_u16();

		IF_VERBOSE_PARSE(log_msg("  tag %d: do_init_action_loader\n", tag_type));
		IF_VERBOSE_ACTION(log_msg("  -- init actions for sprite %d\n", sprite_character_id));

		do_action*	da = new do_action;
		da->read(in);
		m->add_init_action(sprite_character_id, da);
	}


	//
	// action_buffer
	//

	// Disassemble one instruction to the log.
	static void	log_disasm(const unsigned char* instruction_data);

	action_buffer::action_buffer()
		:
		m_decl_dict_processed_at(-1)
	{
	}


	void	action_buffer::read(stream* in)
	{
		// Read action bytes.
		for (;;)
		{
			int	instruction_start = m_buffer.size();

			int	pc = m_buffer.size();

			int	action_id = in->read_u8();
			m_buffer.push_back(action_id);

			if (action_id & 0x80)
			{
				// Action contains extra data.  Read it.
				int	length = in->read_u16();
				m_buffer.push_back(length & 0x0FF);
				m_buffer.push_back((length >> 8) & 0x0FF);
				for (int i = 0; i < length; i++)
				{
					unsigned char	b = in->read_u8();
					m_buffer.push_back(b);
				}
			}

			IF_VERBOSE_ACTION(log_msg("%4d\t", pc); log_disasm(&m_buffer[instruction_start]); );

			if (action_id == 0)
			{
				// end of action buffer.
				break;
			}
		}
	}


	void	action_buffer::process_decl_dict(int start_pc, int stop_pc)
	// Interpret the decl_dict opcode.  Don't read stop_pc or
	// later.  A dictionary is some static strings embedded in the
	// action buffer; there should only be one dictionary per
	// action buffer.
	//
	// NOTE: Normally the dictionary is declared as the first
	// action in an action buffer, but I've seen what looks like
	// some form of copy protection that amounts to:
	//
	// <start of action buffer>
	//          push true
	//          branch_if_true label
	//          decl_dict   [0]   // this is never executed, but has lots of orphan data declared in the opcode
	// label:   // (embedded inside the previous opcode; looks like an invalid jump)
	//          ... "protected" code here, including the real decl_dict opcode ...
	//          <end of the dummy decl_dict [0] opcode>
	//
	// So we just interpret the first decl_dict we come to, and
	// cache the results.  If we ever hit a different decl_dict in
	// the same action_buffer, then we log an error and ignore it.
	{
		assert(stop_pc <= m_buffer.size());

		if (m_decl_dict_processed_at == start_pc)
		{
			// We've already processed this decl_dict.
			int	count = m_buffer[start_pc + 3] | (m_buffer[start_pc + 4] << 8);
			assert(m_dictionary.size() == count);
			UNUSED(count);
			return;
		}

		if (m_decl_dict_processed_at != -1)
		{
			log_error("error: process_decl_dict(%d, %d): decl_dict was already processed at %d\n",
				  start_pc,
				  stop_pc,
				  m_decl_dict_processed_at);
			return;
		}

		m_decl_dict_processed_at = start_pc;

		// Actual processing.
		int	i = start_pc;
		int	length = m_buffer[i + 1] | (m_buffer[i + 2] << 8);
		int	count = m_buffer[i + 3] | (m_buffer[i + 4] << 8);
		i += 2;

		UNUSED(length);

		assert(start_pc + 3 + length == stop_pc);

		m_dictionary.resize(count);

		// Index the strings.
		for (int ct = 0; ct < count; ct++)
		{
			// Point into the current action buffer.
			m_dictionary[ct] = (const char*) &m_buffer[3 + i];

			while (m_buffer[3 + i])
			{
				// safety check.
				if (i >= stop_pc)
				{
					log_error("error: action buffer dict length exceeded\n");

					// Jam something into the remaining (invalid) entries.
					while (ct < count)
					{
						m_dictionary[ct] = "<invalid>";
						ct++;
					}
					return;
				}
				i++;
			}
			i++;
		}
	}


	void	action_buffer::execute(as_environment* env)
	// Interpret the actions in this action buffer, and evaluate
	// them in the given environment.  Execute our whole buffer,
	// without any arguments passed in.
	{
		int	local_stack_top = env->get_local_frame_top();
		env->add_frame_barrier();

		array<with_stack_entry>	empty_with_stack;
		execute(env, 0, m_buffer.size(), NULL, empty_with_stack, false /* not function2 */);

		env->set_local_frame_top(local_stack_top);
	}


	void	action_buffer::execute(
		as_environment* env,
		int start_pc,
		int exec_bytes,
		as_value* retval,
		const array<with_stack_entry>& initial_with_stack,
		bool is_function2)
	// Interpret the specified subset of the actions in our
	// buffer.  Caller is responsible for cleaning up our local
	// stack frame (it may have passed its arguments in via the
	// local stack frame).
	//
	// [DICE-MOD SwiffPlayer #55] is_function2 no longer selects a register set: every register
	// operand addresses the environment's one bank, as in the shipped execute (0x100925A0).
	{
		(void)is_function2;
		action_init();	// @@ stick this somewhere else; need some global static init function

		assert(env);

		array<with_stack_entry>	with_stack(initial_with_stack);

		// Some corner case behaviors depend on the SWF file version.
		int version = env->get_target()->get_movie_definition()->get_version();

#if 0
		// Check the time
		if (periodic_events.expired()) {
			periodic_events.poll_event_handlers(env);
		}
#endif
		
		movie*	original_target = env->get_target();
		UNUSED(original_target);		// Avoid warnings.

		int	stop_pc = start_pc + exec_bytes;

		for (int pc = start_pc; pc < stop_pc; )
		{
			// Cleanup any expired "with" blocks.
			while (with_stack.size() > 0
			       && pc >= with_stack.back().m_block_end_pc)
			{
				// Drop this stack element
				with_stack.resize(with_stack.size() - 1);
			}

			// Get the opcode.
			int	action_id = m_buffer[pc];
			if ((action_id & 0x80) == 0)
			{
				IF_VERBOSE_ACTION(log_msg("EX:\t"); log_disasm(&m_buffer[pc]));

				// IF_VERBOSE_ACTION(log_msg("Action ID is: 0x%x\n", action_id));
			
				// Simple action; no extra data.
				switch (action_id)
				{
				default:
					break;

				case 0x00:	// end of actions.
					return;

				case 0x04:	// next frame.
					env->get_target()->goto_frame(env->get_target()->get_current_frame() + 1);
					break;

				case 0x05:	// prev frame.
					env->get_target()->goto_frame(env->get_target()->get_current_frame() - 1);
					break;

				case 0x06:	// action play
					env->get_target()->set_play_state(movie::PLAY);
					break;

				case 0x07:	// action stop
					env->get_target()->set_play_state(movie::STOP);
					break;

				case 0x08:	// toggle quality
				case 0x09:	// stop sounds
					break;

				case 0x0A:	// add
				{
					env->top(1) += env->top(0);
					env->drop(1);
					break;
				}
				case 0x0B:	// subtract
				{
					env->top(1) -= env->top(0);
					env->drop(1);
					break;
				}
				case 0x0C:	// multiply
				{
					env->top(1) *= env->top(0);
					env->drop(1);
					break;
				}
				case 0x0D:	// divide
				{
					env->top(1) /= env->top(0);
					env->drop(1);
					break;
				}
				case 0x0E:	// equal
				{
					env->top(1).set_bool(env->top(1) == env->top(0));
					env->drop(1);
					break;
				}
				case 0x0F:	// less than
				{
					env->top(1).set_bool(env->top(1) < env->top(0));
					env->drop(1);
					break;
				}
				case 0x10:	// logical and
				{
					env->top(1).set_bool(env->top(1).to_bool() && env->top(0).to_bool());
					env->drop(1);
					break;
				}
				case 0x11:	// logical or
				{
					env->top(1).set_bool(env->top(1).to_bool() && env->top(0).to_bool());
					env->drop(1);
					break;
				}
				case 0x12:	// logical not
				{
					env->top(0).set_bool(! env->top(0).to_bool());
					break;
				}
				case 0x13:	// string equal
				{
					env->top(1).set_bool(env->top(1).to_tu_string() == env->top(0).to_tu_string());
					env->drop(1);
					break;
				}
				case 0x14:	// string length
				{
					env->top(0).set_int(env->top(0).to_tu_string_versioned(version).utf8_length());
					break;
				}
				case 0x15:	// substring
				{
					int	size = int(env->top(0).to_number());
					int	base = int(env->top(1).to_number()) - 1;  // 1-based indices
					const tu_string&	str = env->top(2).to_tu_string_versioned(version);

					// Keep base within range.
					base = iclamp(base, 0, str.length());

					// Truncate if necessary.
					size = imin(str.length() - base, size);

					// @@ This can be done without new allocations if we get dirtier w/ internals
					// of as_value and tu_string...
					tu_string	new_string = str.c_str() + base;
					new_string.resize(size);

					env->drop(2);
					env->top(0).set_tu_string(new_string);

					break;
				}
				case 0x17:	// pop
				{
					env->drop(1);
					break;
				}
				case 0x18:	// int
				{
					env->top(0).set_int(int(floor(env->top(0).to_number())));
					break;
				}
				case 0x1C:	// get variable
				{
					as_value var_name = env->pop();
					tu_string var_string = var_name.to_tu_string();

					as_value variable = env->get_variable(var_string, with_stack);
					env->push(variable);
					if (variable.to_object() == NULL) {
						IF_VERBOSE_ACTION(log_msg("-- get var: %s=%s\n",
									  var_string.c_str(),
									  variable.to_tu_string().c_str()));
					} else {
						IF_VERBOSE_ACTION(log_msg("-- get var: %s=%s at %p\n",
									  var_string.c_str(),
									  variable.to_tu_string().c_str(), variable.to_object()));
					}
					

					break;
				}
				case 0x1D:	// set variable
				{
					env->set_variable(env->top(1).to_tu_string(), env->top(0), with_stack);
					IF_VERBOSE_ACTION(log_msg("-- set var: %s \n",
								  env->top(1).to_tu_string().c_str()));

					env->drop(2);
					break;
				}
				case 0x20:	// set target expression
				{
					as_object_interface* target_object = env->top(0).to_object();

					IF_VERBOSE_ACTION(log_msg("-- ActionSetTarget2: %s (%d)",
								  ((character *) target_object)->m_name.c_str(),
								  ((character *) target_object)->m_id));

					movie* target = env->find_target(target_object);
					env->set_target (target);
					break;
				}
				case 0x21:	// string concat
				{
					env->top(1).convert_to_string_versioned(version);
					env->top(1).string_concat(env->top(0).to_tu_string_versioned(version));
					env->drop(1);
					break;
				}
				case 0x22:	// get property
				{

					movie*	target = env->find_target(env->top(1));
					if (target)
					{
						env->top(1) = get_property(target, (int) env->top(0).to_number());
					}
					else
					{
						env->top(1) = as_value();
					}
					env->drop(1);
					break;
				}

				case 0x23:	// set property
				{

					movie*	target = env->find_target(env->top(2));
					if (target)
					{
						set_property(target, (int) env->top(1).to_number(), env->top(0));
					}
					env->drop(3);
					break;
				}

				case 0x24:	// duplicate clip (sprite?)
				{
					env->get_target()->clone_display_object(
						env->top(2).to_tu_string(),
						env->top(1).to_tu_string(),
						(int) env->top(0).to_number());
					env->drop(3);
					break;
				}

				case 0x25:	// remove clip
					env->get_target()->remove_display_object(env->top(0).to_tu_string());
					env->drop(1);
					break;

				case 0x26:	// trace
				{
					// Log the stack val.
					as_global_trace(fn_call(&env->top(0), NULL, env, 1, env->get_top_index()));
					env->drop(1);
					break;
				}

				case 0x27:	// start drag movie
				{
					movie::drag_state	st;

					st.m_character = env->find_target(env->top(0));
					if (st.m_character == NULL)
					{
						log_error("error: start_drag of invalid target '%s'.\n",
							  env->top(0).to_string());
					}

					st.m_lock_center = env->top(1).to_bool();
					st.m_bound = env->top(2).to_bool();
					if (st.m_bound)
					{
						st.m_bound_x0 = (float) env->top(6).to_number();
						st.m_bound_y0 = (float) env->top(5).to_number();
						st.m_bound_x1 = (float) env->top(4).to_number();
						st.m_bound_y1 = (float) env->top(3).to_number();
						env->drop(4);
					}
					env->drop(3);

					movie*	root_movie = env->get_target()->get_root_movie();
					assert(root_movie);

					if (root_movie && st.m_character)
					{
						root_movie->set_drag_state(st);
					}
					
					break;
				}

				case 0x28:	// stop drag movie
				{
					movie*	root_movie = env->get_target()->get_root_movie();
					assert(root_movie);

					root_movie->stop_drag();

					break;
				}

				case 0x29:	// string less than
				{
					env->top(1).set_bool(env->top(1).to_tu_string() < env->top(0).to_tu_string());
					break;
				}

				case 0x2A:	// throw
				{
					log_error("todo opcode: %02X\n", action_id);
					break;
				}

				case 0x2B:	// cast_object
				{
					// TODO
					//
					// Pop o1, pop s2
					// Make sure o1 is an instance of s2.
					// If the cast succeeds, push o1, else push NULL.
					//
					// The cast doesn't appear to coerce at all, it's more
					// like a dynamic_cast<> in C++ I think.
					log_error("todo opcode: %02X\n", action_id);
					break;
				}

				case 0x2C:	// implements
				{
					// Declare that a class s1 implements one or more
					// interfaces (i2 == number of interfaces, s3..sn are the names
					// of the interfaces).
					log_error("todo opcode: %02X\n", action_id);
					break;
				}

				case 0x30:	// random
				{
					int	max = int(env->top(0).to_number());
					if (max < 1) max = 1;
					env->top(0).set_int(tu_random::next_random() % max);
					break;
				}
				case 0x31:	// mb length
				{
					// @@ TODO
					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x32:	// ord
				{
					// ASCII code of first character
					env->top(0).set_int(env->top(0).to_string()[0]);
					break;
				}
				case 0x33:	// chr
				{
					char	buf[2];
					buf[0] = int(env->top(0).to_number());
					buf[1] = 0;
					env->top(0).set_string(buf);
					break;
				}

				case 0x34:	// get timer
					// Push milliseconds since we started playing.
					env->push(floorf(env->m_target->get_timer() * 1000.0f));
					break;

				case 0x35:	// mb substring
				{
					// @@ TODO
					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x37:	// mb chr
				{
					// @@ TODO
					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x3A:	// delete
				{
					// @@ TODO
					
					// Apparently this can be used to remove properties from
					// an object?

					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x3B:	// delete2
				{
					// @@ tulrich: delete is not valid here!  Do we actually just want to 
					// NULL out the object pointer in the environment (to drop the ref)?
					// Should at least check the ref count before deleting anything!!!
//					as_value	obj_name = env->pop();
					as_value obj_ptr = env->get_variable_raw(env->top(0).to_tu_string(), with_stack);
///x					delete obj_ptr.to_object();
// 	 				log_error("%08X\n", obj_ptr.to_object());
					log_error("todo opcode: %02X\n", action_id);
					break;
				}

				case 0x3C:	// set local
				{
					as_value	value = env->pop();
					as_value	varname = env->pop();
					env->set_local(varname.to_tu_string(), value);
					break;
				}

				case 0x3D:	// call function
				{
					as_value	function;
					// [DICE-MOD SwiffPlayer #26: a function resolved through a `with` scope runs with
					// THAT object as `this`. mainMenu.swf's ScrollBar.createChildren does
					//   with (this.m_mcScrollVertical) { attachMovie("arrowUp", "arrowUp", getNextHighestDepth()); ... }
					// Stock 2006 passed this_ptr = NULL, so the sprite builtins fell back to the
					// environment's target - the root, when the method chain was started from a root-level
					// init action - and the arrows were attached to _root at depth 259+ with an identity
					// transform (retail run L). The shipped player attaches them to the with-object.]
					as_object_interface*	with_this = NULL;
					if (env->top(0).get_type() == as_value::STRING)
					{
						// Function is a string; lookup the function.
						const tu_string&	function_name = env->top(0).to_tu_string();
						for (int wi = with_stack.size() - 1; wi >= 0; wi--)
						{
							as_object_interface*	wo = with_stack[wi].m_object.get_ptr();
							as_value	wv;
							if (wo && wo->get_member(tu_stringi(function_name.c_str()), &wv)
							    && (wv.get_type() == as_value::C_FUNCTION || wv.get_type() == as_value::AS_FUNCTION))
							{
								function = wv;
								with_this = wo;
								break;
							}
						}
						if (with_this == NULL)
						{
							function = env->get_variable(function_name, with_stack);
						}

						if (function.get_type() != as_value::C_FUNCTION
						    && function.get_type() != as_value::AS_FUNCTION)
						{
							log_error("error in call_function: '%s' is not a function\n",
								  function_name.c_str());
						}
					}
					else
					{
						// Hopefully the actual function object is here.
						function = env->top(0);
					}
					int	nargs = (int) env->top(1).to_number();
					as_value	result = call_method(function, env, with_this, nargs, env->get_top_index() - 2);
					env->drop(nargs + 1);
					env->top(0) = result;
					break;
				}
				case 0x3E:	// return
				{
					// Put top of stack in the provided return slot, if
					// it's not NULL.
					if (retval)
					{
						*retval = env->top(0);
					}
					env->drop(1);

					// Skip the rest of this buffer (return from this action_buffer).
					pc = stop_pc;

					break;
				}
				case 0x3F:	// modulo
				{
					as_value	result;
					double	y = env->pop().to_number();
					double	x = env->pop().to_number();
					if (y != 0)
					{
//						env->top(1).set_double(fmod(env->top(1).to_bool() && env->top(0).to_bool());
//						env->drop(1);
						result = fmod(x, y);
					}
//					log_error("modulo x=%f, y=%f, z=%f\n",x,y,result.to_number());
					env->push(result);
					break;
				}
				case 0x40:	// new
				{
					as_value	classname = env->pop();
					IF_VERBOSE_ACTION(log_msg("---new object: %s\n",
								  classname.to_tu_string().c_str()));
					int	nargs = (int) env->pop().to_number();
					// [DICE-MOD SwiffPlayer: the shipped NewObject (interpreter sub_100925A0, case
					// 0x40) consults its BUILTIN CONSTRUCTOR TABLE first - the eight names its
					// action_init registers through sub_1008DDE0: XML, XMLNode, String, Array,
					// Boolean, Number, Color, Object - and only then get_variable(). Stock 2006 goes
					// through the variable lookup alone, where a sprite child or variable of the same
					// name shadows the builtin. Reproduced for exactly those eight names.]
					as_value constructor;
					{
						static const char*	s_builtin_ctors[] =
							{ "XML", "XMLNode", "String", "Array", "Boolean", "Number", "Color", "Object" };
						bool	builtin = false;
						for (int bi = 0; bi < (int) (sizeof(s_builtin_ctors) / sizeof(s_builtin_ctors[0])); bi++)
						{
							if (classname.to_tu_string() == s_builtin_ctors[bi])
							{
								as_value	ctor_val;
								if (s_global->get_member(classname.to_tu_stringi(), &ctor_val)
								    && ctor_val.get_type() == as_value::C_FUNCTION)
								{
									constructor = ctor_val;
									builtin = true;
								}
								break;
							}
						}
						if (!builtin)
						{
							constructor = env->get_variable(classname.to_tu_string(), with_stack);
						}
					}
					as_value new_obj;
					if (constructor.get_type() == as_value::C_FUNCTION)
					{
						// C function is responsible for creating the new object and setting members.
						(constructor.to_c_function())(fn_call(&new_obj, NULL, env, nargs, env->get_top_index()));
					}
					else if (as_as_function* ctor_as_func = constructor.to_as_function())
					{
						// This function is being used as a constructor; make sure
						// it has a prototype object.
						ctor_as_func->lazy_create_properties();
						assert(ctor_as_func->m_properties);

						// Set up the prototype.
						as_value	proto;
						ctor_as_func->m_properties->get_member("prototype", &proto);

						assert(proto.to_object() != NULL);

						// Create an empty object, with a ref to the constructor's prototype.
						smart_ptr<as_object>	new_obj_ptr(new as_object(proto.to_object()));

						// Set up the constructor member.
						new_obj_ptr->set_member("constructor", constructor);
						new_obj_ptr->set_member_flags("constructor", 1);
						
						new_obj.set_as_object_interface(new_obj_ptr.get_ptr());

						// Call the actual constructor function; new_obj is its 'this'.
						// We don't need the function result.
						call_method(constructor, env, new_obj_ptr.get_ptr(), nargs, env->get_top_index());
					}
					else
					{
						if (classname != "String") {
							log_error("can't create object with unknown class '%s'\n",
								  classname.to_tu_string().c_str());
							{	// [TEMP] SwiffPlayer diagnostic: what did the lookup return, and what does _global hold?
								as_value	g;
								const bool	in_global = s_global->get_member(classname.to_tu_stringi(), &g);
								as_object_interface*	o = constructor.to_object();
								log_error("[TEMP] new %s: lookup type=%d%s; _global.%s %s type=%d\n",
									  classname.to_tu_string().c_str(), (int) constructor.get_type(),
									  (o && o->to_movie()) ? " (a movie clip)" : "",
									  classname.to_tu_string().c_str(), in_global ? "present" : "absent",
									  in_global ? (int) g.get_type() : -1);
							}
						} else {
							log_msg("Created special String class\n");
						}
					}

					env->drop(nargs);
					env->push(new_obj);
#if 0
					log_msg("new object %s at %p\n", classname.to_tu_string().c_str(), new_obj);
#endif
					break;
				}
				case 0x41:	// declare local
				{
					const tu_string&	varname = env->top(0).to_tu_string();
					env->declare_local(varname);
					env->drop(1);
					break;
				}
				case 0x42:	// init array
				{
					int	array_size = (int) env->pop().to_number();

					//log_msg("xxx init array: size = %d, top of stack = %d\n",
					//	// array_size, env->get_top_index());//xxxxx

// 					// Call the array constructor, to create an empty array.
// 					as_value	result;
// 					as_global_array_ctor(fn_call(&result, NULL, env, 0, env->get_top_index()));

// 					as_object_interface*	ao = result.to_object();
// 					assert(ao);

// 					// @@ TODO Set array size.
// 					// ao->set_length(whatever); or something

// 					// Fill the elements with the initial values from the stack.
// 					as_value	index_number;
// 					for (int i = 0; i < array_size; i++)
// 					{
// 						// @@ TODO a set_member that takes an int or as_value?
// 						index_number.set_int(i);
// 						ao->set_member(index_number.to_string(), env->pop());
// 					}

// 					env->push(result);

// 					//log_msg("xxx init array end: top of stack = %d, trace(top(0)) =",
// 					//	env->get_top_index());//xxxxxxx
					as_global_trace(fn_call(NULL, NULL, env, 1, env->get_top_index()));	//xxxx

					break;
				}
				case 0x43:	// declare object  (ActionInitObject)
				{
					// [DICE-MOD SwiffPlayer: implement the object literal `{k1:v1, k2:v2}`. Stock
					// 2006 gameswf logs "todo opcode: 43" and leaves the operands on the stack, so
					// every AS2 object literal both loses its value and unbalances the stack.
					// menu.swf uses one. Per the SWF spec the stack is
					//     value1, name1, ... valueN, nameN, numProps   (numProps on top)
					// and the result is the new object. See GAMESWF_DIVERGENCES.md.]
					int	num_props = (int) env->pop().to_number();
					smart_ptr<as_object>	obj(new as_object);
					for (int i = 0; i < num_props; i++)
					{
						const tu_stringi	name = env->pop().to_tu_string().to_tu_stringi();
						const as_value		value = env->pop();
						obj->set_member(name, value);
					}
					as_value	result;
					result.set_as_object_interface(obj.get_ptr());
					env->push(result);
					break;
				}
				case 0x44:	// type of
				{
					switch(env->top(0).get_type())
					{
					case as_value::UNDEFINED:
						env->top(0).set_string("undefined");
						break;
					case as_value::STRING:
						env->top(0).set_string("string");
						break;
					case as_value::NUMBER:
						env->top(0).set_string("number");
						break;
					case as_value::BOOLEAN:
						env->top(0).set_string("boolean");
						break;
					case as_value::OBJECT:
						env->top(0).set_string("object");
						break;
					case as_value::NULLTYPE:
						env->top(0).set_string("null");
						break;
					case as_value::AS_FUNCTION:
					case as_value::C_FUNCTION:
						env->top(0).set_string("function");
						break;
					default:
						log_error("typeof unknown type: %02X\n", env->top(0).get_type());
						break;
					}
					break;
				}
				case 0x45:	// get target
				{
					// @@ TODO
					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x46:	// enumerate
				{
					as_value var_name = env->pop();
					const tu_string& var_string = var_name.to_tu_string();

					as_value variable = env->get_variable(var_string, with_stack);

					if (variable.to_object() == NULL)
					{
						break;
					}
					const as_object* object = (as_object*) (variable.to_object());

					// The end of the enumeration
					as_value nullvalue;
					nullvalue.set_null();
					env->push(nullvalue);
					IF_VERBOSE_ACTION(log_msg("---enumerate - push: NULL\n"));

					// [DICE-MOD SwiffPlayer #49] the object's own enumerate (arrays: indices only).
					const_cast<as_object*>(object)->enumerate(env);

					break;
				}
				case 0x47:	// add_t (typed)
				{
					if (env->top(0).get_type() == as_value::STRING
					    || env->top(1).get_type() == as_value::STRING)
					{
						env->top(1).convert_to_string_versioned(version);
						env->top(1).string_concat(env->top(0).to_tu_string_versioned(version));
					}
					else
					{
						env->top(1) += env->top(0);
					}
					env->drop(1);
					break;
				}
				case 0x48:	// less than (typed)
				{
					if (env->top(1).get_type() == as_value::STRING)
					{
						env->top(1).set_bool(env->top(1).to_tu_string() < env->top(0).to_tu_string());
					}
					else
					{
						env->top(1).set_bool(env->top(1) < env->top(0));
					}
					env->drop(1);
					break;
				}
				case 0x49:	// equal (typed)
				{
					// @@ identical to untyped equal, as far as I can tell...
					env->top(1).set_bool(env->top(1) == env->top(0));
					env->drop(1);
					break;
				}
				case 0x4A:	// to number
				{
					env->top(0).convert_to_number();
					break;
				}
				case 0x4B:	// to string
				{
					env->top(0).convert_to_string_versioned(version);
					break;
				}
				case 0x4C:	// dup
					env->push(env->top(0));
					break;
				
				case 0x4D:	// swap
				{
					as_value	temp = env->top(1);
					env->top(1) = env->top(0);
					env->top(0) = temp;
					break;
				}
				case 0x4E:	// get member
				{
					s_last_member_name = env->top(0).to_tu_string();	// [TEMP] RW-89 diagnostic
                                        as_object_interface*    obj = env->top(1).to_object();

                                        // Special case: String has a member "length"
                                        if (obj == NULL
                                            && env->top(1).get_type() == as_value::STRING
                                            && env->top(0).to_tu_stringi() == "length")
					{
						int     len = env->top(1).to_tu_string_versioned(version).utf8_length();
                                                env->top(1).set_int(len);
					}
                                        else
					{
						env->top(1).set_undefined();
						// int	nargs = (int) env->top(1).to_number();
						if (obj) {
							obj->get_member(env->top(0).to_tu_string(), &(env->top(1)));
							if (env->top(1).to_object() == NULL) {
								IF_VERBOSE_ACTION(log_msg("-- get_member %s=%s\n",
											  env->top(0).to_tu_string().c_str(),
											  env->top(1).to_tu_string().c_str()));
							} else {
								IF_VERBOSE_ACTION(log_msg("-- get_member %s=%s at %p\n",
											  env->top(0).to_tu_string().c_str(),
											  env->top(1).to_tu_string().c_str(), env->top(1).to_object()));
							}
						}
						else
						{
							// @@ log error?
						}
					}
                                        env->drop(1);
                                        break;
					
				}
				case 0x4F:	// set member
				{
					as_object_interface*	obj = env->top(2).to_object();
					if (obj)
					{
						obj->set_member(env->top(1).to_tu_string(), env->top(0));
						IF_VERBOSE_ACTION(
							log_msg("-- set_member %s.%s=%s\n",
								env->top(2).to_tu_string().c_str(),
								env->top(1).to_tu_string().c_str(),
								env->top(0).to_tu_string().c_str()));
					}
					else
					{
						// Invalid object, can't set.
						IF_VERBOSE_ACTION(
							log_msg("-- set_member %s.%s=%s on invalid object!\n",
								env->top(2).to_tu_string().c_str(),
								env->top(1).to_tu_string().c_str(),
								env->top(0).to_tu_string().c_str()));
					}
					env->drop(3);
					break;
				}
				case 0x50:	// increment
					env->top(0) += 1;
					break;
				case 0x51:	// decrement
					env->top(0) -= 1;
					break;
				case 0x52:	// call method
				{
					int	nargs = (int) env->top(2).to_number();
					as_value	result;
					const tu_string&	method_name = env->top(0).to_tu_string();
					as_object_interface*	obj = env->top(1).to_object();
					// [DICE-MOD SwiffPlayer: a DIRECT call of a function value - the method name is
					// undefined/empty and the "object" operand is the function itself: `super(args)`,
					// and `f(args)` on a function-valued expression. The shipped CallMethod (interpreter
					// sub_100925A0, case 0x52) tests for the "undefined" name first and calls the function
					// with the CURRENT this (env+8212). Stock 2006 fell through to "call_method
					// 'undefined' on invalid object", so no class constructor ever reached its base.]
					const bool	direct_call = env->top(0).get_type() == as_value::UNDEFINED
						|| method_name.length() == 0;
					if (direct_call
					    && (env->top(1).get_type() == as_value::AS_FUNCTION
						|| env->top(1).get_type() == as_value::C_FUNCTION))
					{
						as_object_interface*	current_this = env->m_this_object
							? env->m_this_object : static_cast<as_object_interface*>(env->m_target);
						result = call_method(env->top(1), env, current_this, nargs, env->get_top_index() - 3);
					}
					else if (direct_call && env->top(1).get_type() == as_value::UNDEFINED)
					{
						// super() in a class whose superclass is not a script function: DICE's
						// `MovieClip` is a no-op C function, so ActionExtends leaves __constructor__
						// undefined and the call has nothing to run. Quietly nothing, as in the
						// shipped player.
					}
					else if (obj)
					{
						as_value	method;
						if (obj->get_member(method_name, &method))
						{
							if (method.get_type() != as_value::C_FUNCTION
							    && method.get_type() != as_value::AS_FUNCTION)
							{
								log_error("error: call_method: '%s' is not a method\n",
									  method_name.c_str());
							}
							else
							{
								// [DICE-MOD SwiffPlayer #30: `super.method(args)`. The `super` value is
								// the parent class's constructor FUNCTION; as an object that is its
								// properties object, whose prototype chain reaches the parent's
								// methods. A method found through that chain runs with the CURRENT
								// `this` (Flash's super call), while a function's OWN member (a static
								// method, Class.helper()) keeps the function object as `this`.]
								as_object_interface*	call_this = obj;
								if (env->top(1).get_type() == as_value::AS_FUNCTION)
								{
									as_object*	props = obj->cast_to_as_object();
									as_member	own;
									if (props == NULL || props->m_members.get(method_name, &own) == false)
									{
										call_this = env->m_this_object
											? env->m_this_object : static_cast<as_object_interface*>(env->m_target);
									}
								}
								result = call_method(
									method,
									env,
									call_this,
									nargs,
									env->get_top_index() - 3);
							}
						}
						else
						{
							log_error("error: call_method can't find method %s (last member read: '%s')\n", method_name.c_str(), s_last_member_name.c_str());
						}
					}
					else if (env->top(1).get_type() == as_value::STRING)
					{
						// Handle methods on literal strings.
						string_method(
							fn_call(&result, NULL, env, nargs, env->get_top_index() - 3),
							method_name.to_tu_stringi(),
							env->top(1).to_tu_string_versioned(version));
					}
					else if (env->top(1).get_type() == as_value::C_FUNCTION)
					{
						// Catch method calls on String
						// constructor.  There may be a cleaner
						// way to do this. Perhaps we call the
						// constructor function with a special flag, to
						// indicate that it's a method call?
						if (env->top(1).to_c_function() == string_ctor)
						{
							tu_string dummy;
							string_method(
								fn_call(&result, NULL, env, nargs, env->get_top_index() - 3),
								method_name.to_tu_stringi(),
								dummy);
						}
						else if (env->top(1).to_c_function() == as_global_object_ctor
							 && method_name == "registerClass")
						{
							// [DICE-MOD SwiffPlayer: Object.registerClass - see object_register_class.]
							object_register_class(
								fn_call(&result, NULL, env, nargs, env->get_top_index() - 3));
						}
						else
						{
							log_error("error: method call on unknown c function.\n");
							// [TEMP] SwiffPlayer diagnostic: which method on which builtin?
							log_error("[TEMP] c-function method call: %s on %s\n", method_name.c_str(),
								  env->top(1).to_c_function() == as_global_object_ctor ? "Object" : "<other c function>");
						}
					}
					else
					{
						if (env->top(1).get_type() == as_value::NUMBER
						    && method_name == "toString")
						{
							// Numbers have a .toString() method.
							result.set_tu_string(env->top(1).to_tu_string());
						}
						else
						{
							log_error("error: call_method '%s' on invalid object (last member read: '%s').\n", method_name.c_str(), s_last_member_name.c_str());
						}
					}
					env->drop(nargs + 2);
					env->top(0) = result;
					break;
				}
				case 0x53:	// new method
				{
					// [DICE-MOD SwiffPlayer: ActionNewMethod - `new obj.Class(args)`. Stock 2006 logged
					// "todo opcode: 53" and left the operands on the stack. Per the SWF spec the stack
					// is `args..., nargs, object, name` (name on top); the constructor is object.name,
					// or the object itself when the name is empty/undefined. The shipped handler
					// (interpreter sub_100925A0, case 0x53) then does what its NewObject does: a fresh
					// object whose __proto__ is Class.prototype, constructor run with it as `this`.
					// Same shape as ActionNewObject (0x40) above.]
					as_value	method_name = env->pop();
					as_value	obj_val = env->pop();
					int	nargs = (int) env->pop().to_number();

					as_value	constructor;
					if (method_name.get_type() == as_value::UNDEFINED
					    || method_name.to_tu_string().length() == 0)
					{
						constructor = obj_val;
					}
					else
					{
						as_object_interface*	o = obj_val.to_object();
						if (o)
						{
							o->get_member(method_name.to_tu_stringi(), &constructor);
						}
					}

					as_value	new_obj;
					if (constructor.get_type() == as_value::C_FUNCTION)
					{
						(constructor.to_c_function())(fn_call(&new_obj, NULL, env, nargs, env->get_top_index()));
					}
					else if (as_as_function* ctor_as_func = constructor.to_as_function())
					{
						ctor_as_func->lazy_create_properties();
						as_value	proto;
						if (ctor_as_func->m_properties)
						{
							ctor_as_func->m_properties->get_member("prototype", &proto);
						}
						smart_ptr<as_object>	new_obj_ptr(new as_object(proto.to_object()));
						new_obj_ptr->set_member("constructor", constructor);
						new_obj_ptr->set_member_flags("constructor", 1);
						new_obj.set_as_object_interface(new_obj_ptr.get_ptr());
						call_method(constructor, env, new_obj_ptr.get_ptr(), nargs, env->get_top_index());
					}
					else
					{
						log_error("error: new method: '%s' is not a constructor\n",
							  method_name.to_tu_string().c_str());
					}

					env->drop(nargs);
					env->push(new_obj);
					break;
				}
				case 0x54:	// instance of
					// @@ TODO
					log_error("todo opcode: %02X\n", action_id);
					break;
				case 0x55:	// enumerate2 [DICE-MOD SwiffPlayer #43]
				{
					// [DICE-MOD SwiffPlayer #43] ActionEnumerate2 (SWF 6): the object itself is on the
					// stack (Enumerate, 0x46, takes a variable NAME). Stock 2006 logged "todo opcode: 55"
					// and pushed nothing - not even the null terminator - so a for-in loop read garbage
					// below the stack and never ended: mainMenu.swf's sort-button toggleCheck() spins
					// through its sibling group with it, and the client hung on the multiplayer tab
					// (interactive run drive3). A non-object still gets the terminator.
					as_value variable = env->pop();

					if (variable.to_object() == NULL)
					{
						as_value nullvalue;
						nullvalue.set_null();
						env->push(nullvalue);
						break;
					}
					const as_object* object = (as_object*) (variable.to_object());

					// The end of the enumeration
					as_value nullvalue;
					nullvalue.set_null();
					env->push(nullvalue);
					IF_VERBOSE_ACTION(log_msg("---enumerate - push: NULL\n"));

					// [DICE-MOD SwiffPlayer #49] the object's own enumerate (arrays: indices only).
					const_cast<as_object*>(object)->enumerate(env);

					break;
				}
				case 0x60:	// bitwise and
					env->top(1) &= env->top(0);
					env->drop(1);
					break;
				case 0x61:	// bitwise or
					env->top(1) |= env->top(0);
					env->drop(1);
					break;
				case 0x62:	// bitwise xor
					env->top(1) ^= env->top(0);
					env->drop(1);
					break;
				case 0x63:	// shift left
					env->top(1).shl(env->top(0));
					env->drop(1);
					break;
				case 0x64:	// shift right (signed)
					env->top(1).asr(env->top(0));
					env->drop(1);
					break;
				case 0x65:	// shift right (unsigned)
					env->top(1).lsr(env->top(0));
					env->drop(1);
					break;
				case 0x66:	// strict equal
					if (env->top(1).get_type() != env->top(0).get_type())
					{
						// Types don't match.
						env->top(1).set_bool(false);
						env->drop(1);
					}
					else
					{
						env->top(1).set_bool(env->top(1) == env->top(0));
						env->drop(1);
					}
					break;
				case 0x67:	// gt (typed)
					if (env->top(1).get_type() == as_value::STRING)
					{
						env->top(1).set_bool(env->top(1).to_tu_string() > env->top(0).to_tu_string());
					}
					else
					{
						env->top(1).set_bool(env->top(1).to_number() > env->top(0).to_number());
					}
					env->drop(1);
					break;
				case 0x68:	// string gt
					env->top(1).set_bool(env->top(1).to_tu_string() > env->top(0).to_tu_string());
					env->drop(1);
					break;

				case 0x69:	// extends  (ActionExtends)
				{
					// [DICE-MOD SwiffPlayer: implement AS2 class inheritance. Stock 2006 gameswf
					// logs "todo opcode: 69" and leaves BOTH operands on the stack; menu.swf runs it
					// three times, so three subclasses lose their prototype chain (their inherited
					// methods then resolve to undefined) and the stack is left 6 deep.
					//
					// Modelled on the shipped handler (SwiffPlayer.dll (2).i64, interpreter
					// sub_100925A0 case 0x69): the SUPERCLASS is popped first (it is on top of the
					// stack), then the subclass; both are converted to functions; then
					//     subclass.prototype.__constructor__ = superclass;
					//     subclass.prototype.__proto__       = superclass.prototype;
					// on the subclass's EXISTING prototype object (nothing is replaced), and a
					// superclass that is not a script function - DICE's `MovieClip` is a no-op C
					// function - simply leaves the chain empty, with no error. The first cut of
					// this divergence popped in the wrong order, which inverted every chain and
					// flagged `extends MovieClip` as "subclass is not an object or function"
					// (retail 2026-09-02). gameswf's as_object prototype pointer IS its __proto__
					// chain (set_member/get_member "__proto__" reach m_prototype). See
					// GAMESWF_DIVERGENCES.md.]
					as_value	superclass = env->pop();
					as_value	subclass = env->pop();

					as_as_function*		sub_fn = subclass.to_as_function();
					as_as_function*		super_fn = superclass.to_as_function();
					if (sub_fn == NULL)
					{
						log_error("error: extends: subclass is not a function\n");
						break;
					}

					sub_fn->lazy_create_properties();
					as_value	sub_proto_val;
					if (sub_fn->m_properties)
					{
						sub_fn->m_properties->get_member("prototype", &sub_proto_val);
					}
					as_object_interface*	sub_proto = sub_proto_val.to_object();
					if (sub_proto == NULL)
					{
						log_error("error: extends: subclass has no prototype object\n");
						break;
					}

					as_value	super_proto_val;
					if (super_fn)
					{
						super_fn->lazy_create_properties();
						if (super_fn->m_properties)
						{
							super_fn->m_properties->get_member("prototype", &super_proto_val);
						}
					}
					sub_proto->set_member("__constructor__", super_fn ? as_value(super_fn) : as_value());
					// This 2006 as_object keeps its __proto__ chain under the member name "prototype"
					// (set_member/get_member "prototype" ARE m_prototype; a member literally named
					// "__proto__" would be an inert plain member - the first cut of this handler set
					// that and no inherited method ever resolved).
					sub_proto->set_member("prototype", super_proto_val);
					break;
				}

				}
				pc++;	// advance to next action.
			}
			else
			{
				IF_VERBOSE_ACTION(log_msg("EX:\t"); log_disasm(&m_buffer[pc]));

				// Action containing extra data.
				int	length = m_buffer[pc + 1] | (m_buffer[pc + 2] << 8);
				int	next_pc = pc + length + 3;

				switch (action_id)
				{
				default:
					break;

				case 0x81:	// goto frame
				{
					int	frame = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
					// 0-based already?
					//// Convert from 1-based to 0-based
					//frame--;
					env->get_target()->goto_frame(frame);
					break;
				}

				case 0x83:	// get url
				{
					// If this is an FSCommand, then call the callback
					// handler, if any.

					// Two strings as args.
					const char*	url = (const char*) &(m_buffer[pc + 3]);
					int	url_len = strlen(url);
					const char*	target = (const char*) &(m_buffer[pc + 3 + url_len + 1]);

					// If the url starts with "FSCommand:", then this is
					// a message for the host app.
					if (strncmp(url, "FSCommand:", 10) == 0)
					{
						if (s_fscommand_handler)
						{
							// Call into the app.
							(*s_fscommand_handler)(env->get_target()->get_root_interface(), url + 10, target);
						}
					}
					else
					{
#ifdef EXTERN_MOVIE
//						log_error("get url: target=%s, url=%s\n", target, url);
            
						tu_string tu_target = target;
						movie* target_movie = env->find_target(tu_target);
						if (target_movie != NULL)
						{
							movie*	root_movie = env->get_target()->get_root_movie();
							attach_extern_movie(url, target_movie, root_movie);
						}
						else
						{
							log_error("get url: target %s not found\n", target);
						}
#endif // EXTERN_MOVIE
					}

					break;
				}

				case 0x87:	// store_register
				{
					int	reg = m_buffer[pc + 3];
					// Save top of stack in specified register.
					// [DICE-MOD SwiffPlayer #55] the environment's bank, for timeline code and function
					// bodies alike - the shipped case 0x87 assigns env+0x10+32*reg (a byte operand).
					*(env->register_ptr(reg)) = env->top(0);

					IF_VERBOSE_ACTION(
						log_msg("-------------- reg[%d] = '%s'\n",
							reg,
							env->top(0).to_string()));

					break;
				}

				case 0x88:	// decl_dict: declare dictionary
				{
					int	i = pc;
					//int	count = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
					i += 2;

					process_decl_dict(pc, next_pc);

					break;
				}

				case 0x8A:	// wait for frame
				{
					// If we haven't loaded a specified frame yet, then we're supposed to skip
					// some specified number of actions.
					//
					// Since we don't load incrementally, just ignore this opcode.
					break;
				}

				case 0x8B:	// set target
				{
					// Change the movie we're working on.
					const char* target_name = (const char*) &m_buffer[pc + 3];
					if (target_name[0] == 0) { env->set_target(original_target); }
					else {
//						env->set_target(env->get_target()->find_labeled_target(target_name));
//						if (env->get_target() == NULL) env->set_target(original_target);
					}
					break;
				}

				case 0x8C:	// go to labeled frame, goto_frame_lbl
				{
					char*	frame_label = (char*) &m_buffer[pc + 3];
					env->get_target()->goto_labeled_frame(frame_label);
					break;
				}

				case 0x8D:	// wait for frame expression (?)
				{
					// Pop the frame number to wait for; if it's not loaded skip the
					// specified number of actions.
					//
					// Since we don't support incremental loading, pop our arg and
					// don't do anything.
					env->drop(1);
					break;
				}

				case 0x8E:	// function2
				{
					as_as_function*	func = new as_as_function(this, env, next_pc, with_stack);
					func->set_is_function2();

					int	i = pc;
					i += 3;

					// Extract name.
					// @@ security: watch out for possible missing terminator here!
					tu_string	name = (const char*) &m_buffer[i];
					i += name.length() + 1;

					// Get number of arguments.
					int	nargs = m_buffer[i] | (m_buffer[i + 1] << 8);
					i += 2;

					// Get the count of local registers used by this function.
					uint8	register_count = m_buffer[i];
					i += 1;
					func->set_local_register_count(register_count);

					// Flags, for controlling register assignment of implicit args.
					uint16	flags = m_buffer[i] | (m_buffer[i + 1] << 8);
					i += 2;
					func->set_function2_flags(flags);

					// Get the register assignments and names of the arguments.
					for (int n = 0; n < nargs; n++)
					{
						int	arg_register = m_buffer[i];
						i++;

						// @@ security: watch out for possible missing terminator here!
						func->add_arg(arg_register, (const char*) &m_buffer[i]);
						i += func->m_args.back().m_name.length() + 1;
					}

					// Get the length of the actual function code.
					int	length = m_buffer[i] | (m_buffer[i + 1] << 8);
					i += 2;
					func->set_length(length);

					// Skip the function body (don't interpret it now).
					next_pc += length;

					// If we have a name, then save the function in this
					// environment under that name.
					as_value	function_value(func);
					if (name.length() > 0)
					{
						// @@ NOTE: should this be m_target->set_variable()???
						env->set_member(name, function_value);
					}

					// Also leave it on the stack.
					env->push_val(function_value);

					break;
				}

				case 0x94:	// with
				{
					int	frame = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
					UNUSED(frame);
					IF_VERBOSE_ACTION(log_msg("-------------- with block start: stack size is %d\n", with_stack.size()));
					if (with_stack.size() < 8)
					{
 						int	block_length = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
 						int	block_end = next_pc + block_length;
 						as_object_interface*	with_obj = env->top(0).to_object();
 						with_stack.push_back(with_stack_entry(with_obj, block_end));
					}
					env->drop(1);
					break;
				}
				case 0x96:	// push_data
				{
					int i = pc;
					while (i - pc < length)
					{
						int	type = m_buffer[3 + i];
						i++;
						if (type == 0)
						{
							// string
							const char*	str = (const char*) &m_buffer[3 + i];
							i += strlen(str) + 1;
							env->push(str);

							IF_VERBOSE_ACTION(log_msg("-------------- pushed '%s'\n", str));
						}
						else if (type == 1)
						{
							// float (little-endian)
							union {
								float	f;
								Uint32	i;
							} u;
							compiler_assert(sizeof(u) == sizeof(u.i));

							memcpy(&u.i, &m_buffer[3 + i], 4);
							u.i = swap_le32(u.i);
							i += 4;

							env->push(u.f);

							IF_VERBOSE_ACTION(log_msg("-------------- pushed '%f'\n", u.f));
						}
						else if (type == 2)
						{
							as_value nullvalue;
							nullvalue.set_null();
							env->push(nullvalue);	

							IF_VERBOSE_ACTION(log_msg("-------------- pushed NULL\n"));
						}
						else if (type == 3)
						{
							env->push(as_value());

							IF_VERBOSE_ACTION(log_msg("-------------- pushed UNDEFINED\n"));
						}
						else if (type == 4)
						{
							// contents of register
							int	reg = m_buffer[3 + i];
							i++;
							// [DICE-MOD SwiffPlayer #55] the environment's bank for all code - the
							// shipped push type 4 reads env+0x10+32*reg.
							env->push(*(env->register_ptr(reg)));
							IF_VERBOSE_ACTION(
								log_msg("-------------- pushed register[%d] = '%s'\n",
									reg,
									env->top(0).to_string()));

						}
						else if (type == 5)
						{
							bool	bool_val = m_buffer[3 + i] ? true : false;
							i++;
//							log_msg("bool(%d)\n", bool_val);
							env->push(bool_val);

							IF_VERBOSE_ACTION(log_msg("-------------- pushed %s\n", bool_val ? "true" : "false"));
						}
						else if (type == 6)
						{
							// double
							// wacky format: 45670123
							union {
								double	d;
								Uint64	i;
								struct {
									Uint32	lo;
									Uint32	hi;
								} sub;
							} u;
							compiler_assert(sizeof(u) == sizeof(u.i));

							memcpy(&u.sub.hi, &m_buffer[3 + i], 4);
							memcpy(&u.sub.lo, &m_buffer[3 + i + 4], 4);
							u.i = swap_le64(u.i);
							i += 8;

							env->push(u.d);

							IF_VERBOSE_ACTION(log_msg("-------------- pushed double %f\n", u.d));
						}
						else if (type == 7)
						{
							// int32
							Sint32	val = m_buffer[3 + i]
								| (m_buffer[3 + i + 1] << 8)
								| (m_buffer[3 + i + 2] << 16)
								| (m_buffer[3 + i + 3] << 24);
							i += 4;
						
							env->push(val);

							IF_VERBOSE_ACTION(log_msg("-------------- pushed int32 %d\n", val));
						}
						else if (type == 8)
						{
							int	id = m_buffer[3 + i];
							i++;
							if (id < m_dictionary.size())
							{
								env->push(m_dictionary[id]);

								IF_VERBOSE_ACTION(log_msg("-------------- pushed '%s'\n", m_dictionary[id]));
							}
							else
							{
								log_error("error: dict_lookup(%d) is out of bounds!\n", id);
								env->push(0);
								IF_VERBOSE_ACTION(log_msg("-------------- pushed 0\n"));
							}
						}
						else if (type == 9)
						{
							int	id = m_buffer[3 + i] | (m_buffer[4 + i] << 8);
							i += 2;
							if (id < m_dictionary.size())
							{
								env->push(m_dictionary[id]);
								IF_VERBOSE_ACTION(log_msg("-------------- pushed '%s'\n", m_dictionary[id]));
							}
							else
							{
								log_error("error: dict_lookup(%d) is out of bounds!\n", id);
								env->push(0);

								IF_VERBOSE_ACTION(log_msg("-------------- pushed 0"));
							}
						}
					}
					
					break;
				}
				case 0x99:	// branch always (goto)
				{
					Sint16	offset = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
					next_pc += offset;
					// @@ TODO range checks
					break;
				}
				case 0x9A:	// get url 2
				{
					int	method = m_buffer[pc + 3];
					UNUSED(method);

					const char*	target = env->top(0).to_string();
					const char*	url = env->top(1).to_string();

					// If the url starts with "FSCommand:", then this is
					// a message for the host app.
					if (strncmp(url, "FSCommand:", 10) == 0)
					{
						if (s_fscommand_handler)
						{
							// Call into the app.
							(*s_fscommand_handler)(env->get_target()->get_root_interface(), url + 10, target);
						}
					}
					else
					{
#ifdef EXTERN_MOVIE
//            log_error("get url2: target=%s, url=%s\n", target, url);

						movie* target_movie = env->find_target(env->top(0));
						if (target_movie != NULL)
						{
							movie*	root_movie = env->get_target()->get_root_movie();
							attach_extern_movie(url, target_movie, root_movie);
						}
						else
						{
							log_error("get url2: target %s not found\n", target);
						}
#endif // EXTERN_MOVIE
					}
					env->drop(2);
					break;
				}

				case 0x9B:	// declare function
				{
					as_as_function*	func = new as_as_function(this, env, next_pc, with_stack);

					int	i = pc;
					i += 3;

					// Extract name.
					// @@ security: watch out for possible missing terminator here!
					tu_string	name = (const char*) &m_buffer[i];
					i += name.length() + 1;

					// Get number of arguments.
					int	nargs = m_buffer[i] | (m_buffer[i + 1] << 8);
					i += 2;

					// Get the names of the arguments.
					for (int n = 0; n < nargs; n++)
					{
						// @@ security: watch out for possible missing terminator here!
						func->add_arg(0, (const char*) &m_buffer[i]);
						i += func->m_args.back().m_name.length() + 1;
					}

					// Get the length of the actual function code.
					int	length = m_buffer[i] | (m_buffer[i + 1] << 8);
					i += 2;
					func->set_length(length);

					// Skip the function body (don't interpret it now).
					next_pc += length;

					// If we have a name, then save the function in this
					// environment under that name.
					as_value	function_value(func);
					if (name.length() > 0)
					{
						// @@ NOTE: should this be m_target->set_variable()???
						env->set_member(name, function_value);
					}

					// Also leave it on the stack.
					env->push_val(function_value);

					break;
				}

				case 0x9D:	// branch if true
				{
					Sint16	offset = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
					
					bool	test = env->top(0).to_bool();
					env->drop(1);
					if (test)
					{
						next_pc += offset;

						if (next_pc > stop_pc)
						{
							log_error("branch to offset %d -- this section only runs to %d\n",
								  next_pc,
								  stop_pc);
						}
					}
					break;
				}
				case 0x9E:	// call frame
				{
					// Note: no extra data in this instruction!
					assert(env->m_target);
					env->m_target->call_frame_actions(env->top(0));
					env->drop(1);

					break;
				}

				case 0x9F:	// goto frame expression, goto_frame_exp
				{
					// From Alexi's SWF ref:
					//
					// Pop a value or a string and jump to the specified
					// frame. When a string is specified, it can include a
					// path to a sprite as in:
					// 
					//   /Test:55
					// 
					// When f_play is ON, the action is to play as soon as
					// that frame is reached. Otherwise, the
					// frame is shown in stop mode.

					unsigned char	play_flag = m_buffer[pc + 3];
					movie::play_state	state = play_flag ? movie::PLAY : movie::STOP;

					movie* target = env->get_target();
					bool success = false;

					if (env->top(0).get_type() == as_value::UNDEFINED)
					{
						// No-op.
					}
					else if (env->top(0).get_type() == as_value::STRING)
					{
						// @@ TODO: parse possible sprite path...
						
						// Also, if the frame spec is actually a number (not a label), then
						// we need to do the conversion...

						const char* frame_label = env->top(0).to_string();
						if (target->goto_labeled_frame(frame_label))
						{
							success = true;
						}
						else
						{
							// Couldn't find the label.  Try converting to a number.
							double num;
							if (string_to_number(&num, env->top(0).to_string()))
							{
								int frame_number = int(num);
								target->goto_frame(frame_number);
								success = true;
							}
							// else no-op.
						}
					}
					else if (env->top(0).get_type() == as_value::OBJECT)
					{
						// This is a no-op; see test_goto_frame.swf
					}
					else if (env->top(0).get_type() == as_value::NUMBER)
					{
						// Frame numbers appear to be 0-based!  @@ Verify.
						int frame_number = int(env->top(0).to_number());
						target->goto_frame(frame_number);
						success = true;
					}

					if (success)
					{
						target->set_play_state(state);
					}
					
					env->drop(1);

					break;
				}
				
				}
				pc = next_pc;
			}
		}

		env->set_target(original_target);
	}


	//
	// as_value -- ActionScript value type
	//

	
	as_value::as_value(as_object_interface* obj)
		:
		m_type(OBJECT),
		m_object_value(obj)
	{
		if (m_object_value)
		{
			m_object_value->add_ref();
		}
	}


	as_value::as_value(as_as_function* func)
		:
		m_type(AS_FUNCTION),
		m_as_function_value(func)
	{
		if (m_as_function_value)
		{
			m_as_function_value->add_ref();
		}
	}


	const char*	as_value::to_string() const
	// Conversion to string.
	{
		return to_tu_string().c_str();
	}


	const tu_stringi&	as_value::to_tu_stringi() const
	{
		return reinterpret_cast<const tu_stringi&>(to_tu_string());
	}


	const tu_string&	as_value::to_tu_string() const
	// Conversion to const tu_string&.
	{
		if (m_type == STRING) { /* don't need to do anything */ }
		else if (m_type == NUMBER)
		{
			// @@ Moock says if value is a NAN, then result is "NaN"
			// INF goes to "Infinity"
			// -INF goes to "-Infinity"
			char buffer[50];
			snprintf(buffer, 50, "%.14g", m_number_value);
			m_string_value = buffer;
		}
		else if (m_type == UNDEFINED)
		{
			// Behavior depends on file version.  In
			// version 7+, it's "undefined", in versions
			// 6-, it's "".
			//
			// We'll go with the v7 behavior by default,
			// and conditionalize via _versioned()
			// functions.
			m_string_value = "undefined";
		}
		else if (m_type == NULLTYPE)
		{ 
			m_string_value = "null";
		}
		else if (m_type == BOOLEAN)
		{
			m_string_value = this->m_boolean_value ? "true" : "false";
		}
		else if (m_type == OBJECT)
		{
			// @@ Moock says, "the value that results from
			// calling toString() on the object".
			//
			// The default toString() returns "[object
			// Object]" but may be customized.
			//
			// A Movieclip returns the absolute path of the object.

			const char*	val = NULL;
			if (m_object_value)
			{
				val = m_object_value->get_text_value();
			}

			if (val)
			{
				m_string_value = val;
			}
			else
			{
				// Do we have a "toString" method?
				//
				// TODO: we need an environment in order to call toString()!

				// [DICE-MOD SwiffPlayer #50] The shipped to_tu_string (gameswf_as_value_to_tu_string,
				// 0x1008B430, the OBJECT arm): an object with no text value converts to
				// "<object 0x%X>" carrying its own address (a 50-byte _snprintf), never to
				// "[object Object]" - which is what makes the shipped operator== tell two clips
				// apart. The function arms format "<c_function 0x%X>" / "<as_function 0x%X>" the
				// same way there; they are not on any measured path and stay stock here.
				// [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): %zX prints the
				// whole address, so two objects never alias through their low 32 bits; on x86 it is
				// the shipped %X of the 32-bit address. The three function/object arms share this.
				char	buf[52];
				_snprintf(buf, 50, "<object 0x%zX>", (size_t) (uintptr_t) m_object_value);
				buf[50] = 0;
				m_string_value = buf;
			}
		}
		else if (m_type == C_FUNCTION)
		{
			char buffer[50];
			snprintf(buffer, 50, "<c_function 0x%zX>", (size_t) (uintptr_t) m_c_function_value);
			m_string_value = buffer;
		}
		else if (m_type == AS_FUNCTION)
		{
			char buffer[50];
			snprintf(buffer, 50, "<as_function 0x%zX>", (size_t) (uintptr_t) m_as_function_value);
			m_string_value = buffer;
		}
		else
		{
			m_string_value = "<bad type>";
			assert(0);
		}
		
		return m_string_value;
	}


	const tu_string&	as_value::to_tu_string_versioned(int version) const
	// Conversion to const tu_string&.
	{
		if (m_type == UNDEFINED)
		{
			// Version-dependent behavior.
			if (version <= 6)
			{
				m_string_value = "";
			}
			else
			{
				m_string_value = "undefined";
			}
			return m_string_value;
		}
		
		return to_tu_string();
	}

	double	as_value::to_number() const
	// Conversion to double.
	{
		if (m_type == STRING)
		{
			// @@ Moock says the rule here is: if the
			// string is a valid float literal, then it
			// gets converted; otherwise it is set to NaN.
			//
			// Also, "Infinity", "-Infinity", and "NaN"
			// are recognized.
			if (! string_to_number(&m_number_value, m_string_value.c_str()))
			{
				// Failed conversion to Number.
				m_number_value = 0.0;	// TODO should be NaN
			}
			return m_number_value;
		}
		else if (m_type == NULLTYPE)
		{
 			// Evan: from my tests
			return 0;
		}
		else if (m_type == BOOLEAN)
		{
			// Evan: from my tests
			return (this->m_boolean_value) ? 1 : 0;
		}
		else if (m_type == NUMBER)
		{
			return m_number_value;
		}
		else if (m_type == OBJECT && m_object_value != NULL)
		{
			// @@ Moock says the result here should be
			// "the return value of the object's valueOf()
			// method".
			//
			// Arrays and Movieclips should return NaN.

			// Text characters with var names could get in
			// here.
			const char* textval = m_object_value->get_text_value();
			if (textval)
			{
				return atof(textval);
			}

			return 0.0;
		}
		else
		{
			return 0.0;
		}
	}


	bool	as_value::to_bool() const
	// Conversion to boolean.
	{
		// From Moock
		if (m_type == STRING)
		{
			if (m_string_value == "false")
			{
				return false;
			}
			else if (m_string_value == "true")
			{
				return true;
			}
			else
			{
				// @@ Moock: "true if the string can
				// be converted to a valid nonzero
				// number".
				//
				// Empty string --> false
				return to_number() != 0.0;
			}
		}
		else if (m_type == NUMBER)
		{
			// @@ Moock says, NaN --> false
			return m_number_value != 0.0;
		}
		else if (m_type == BOOLEAN)
		{
			return this->m_boolean_value;
		}
		else if (m_type == OBJECT)
		{
			return m_object_value != NULL;
		}
		else if (m_type == C_FUNCTION)
		{
			return m_c_function_value != NULL;
		}
		else if (m_type == AS_FUNCTION)
		{
			return m_as_function_value != NULL;
		}
		else
		{
			assert(m_type == UNDEFINED || m_type == NULLTYPE);
			return false;
		}
	}

	
	as_object_interface*	as_value::to_object() const
	// Return value as an object.
	{
		if (m_type == OBJECT)
		{
			// OK.
			return m_object_value;
		}
		else if (m_type == AS_FUNCTION && m_as_function_value != NULL)
		{
			// Make sure this as_function has properties &
			// a prototype object attached to it, since those
			// may be about to be referenced.
			m_as_function_value->lazy_create_properties();
			assert(m_as_function_value->m_properties);

			return m_as_function_value->m_properties;
		}
		else
		{
			return NULL;
		}
	}


	as_c_function_ptr	as_value::to_c_function() const
	// Return value as a C function ptr.  Returns NULL if value is
	// not a C function.
	{
		if (m_type == C_FUNCTION)
		{
			// OK.
			return m_c_function_value;
		}
		else
		{
			return NULL;
		}
	}

	as_as_function*	as_value::to_as_function() const
	// Return value as an ActionScript function.  Returns NULL if value is
	// not an ActionScript function.
	{
		if (m_type == AS_FUNCTION)
		{
			// OK.
			return m_as_function_value;
		}
		else
		{
			return NULL;
		}
	}


	void	as_value::convert_to_number()
	// Force type to number.
	{
		set_double(to_number());
	}


	void	as_value::convert_to_string()
	// Force type to string.
	{
		to_tu_string();	// init our string data.
		m_type = STRING;	// force type.
	}


	void	as_value::convert_to_string_versioned(int version)
	// Force type to string.
	{
		to_tu_string_versioned(version);	// init our string data.
		m_type = STRING;	// force type.
	}


	void	as_value::set_as_object_interface(as_object_interface* obj)
	{
		if (m_type != OBJECT || m_object_value != obj)
		{
			drop_refs();
			m_type = OBJECT;
			m_object_value = obj;
			if (m_object_value)
			{
				m_object_value->add_ref();
			}
		}
	}

	void	as_value::set_as_as_function(as_as_function* func)
	{
		if (m_type != AS_FUNCTION || m_as_function_value != func)
		{
			drop_refs();
			m_type = AS_FUNCTION;
			m_as_function_value = func;
			if (m_as_function_value)
			{
				m_as_function_value->add_ref();
			}
		}
	}


	bool	as_value::operator==(const as_value& v) const
	// Return true if operands are equal.
	// [DICE-MOD SwiffPlayer #50] The shipped as_value::operator== (sub_1008B800), arm by arm on
	// THIS operand's type: a string compares with the other's string conversion; a number with the
	// other's to_number; a boolean by both sides' to_bool; an OBJECT by the two string conversions
	// (a text-less object converts to "<object 0x%X>" with its own address - see to_tu_string - so
	// distinct clips differ and the same clip matches); a null object compares an undefined value's
	// string ("undefined") with the other's; everything else (undefined, both function types) is
	// equal exactly when the types match. Stock 2006 answered `m_type == v.m_type` for objects, so
	// every clip equalled every other: mainMenu.swf's Radio.toggleCheck skipped all its siblings
	// as `other != this` was never true, and the difficulty radios could all be lit at once.
	// The shipped player has ONE type for null and undefined (its enum has no null; `Push null` and
	// the enumerate terminator are both type 0), so null == null and null == undefined hold there
	// through the type-match arm. This port keeps two types, so stock 2006's null/undefined arm
	// stays in front to give the same answers; the one shipped quirk it does not reproduce is a
	// STRING "undefined" comparing equal to an undefined value.
	{
		bool this_nulltype = (m_type == UNDEFINED || m_type == NULLTYPE);
		bool v_nulltype = (v.get_type() == UNDEFINED || v.get_type() == NULLTYPE);
		if (this_nulltype || v_nulltype)
		{
			return this_nulltype == v_nulltype;
		}
		else if (m_type == STRING)
		{
			return m_string_value == v.to_tu_string();
		}
		else if (m_type == NUMBER)
		{
			return m_number_value == v.to_number();
		}
		else if (m_type == BOOLEAN)
		{
			return to_bool() == v.to_bool();
		}
		else if (m_type == OBJECT && m_object_value)
		{
			return to_tu_string() == v.to_tu_string();
		}
		else if (m_type == OBJECT)
		{
			// a null-pointer object: the shipped compares an undefined value's string with the other's
			as_value	undef;
			return undef.to_tu_string() == v.to_tu_string();
		}
		else
		{
			return m_type == v.m_type;
		}
	}

	
	bool	as_value::operator!=(const as_value& v) const
	// Return true if operands are not equal.
	{
		return ! (*this == v);
	}

	
	void	as_value::string_concat(const tu_string& str)
	// Sets *this to this string plus the given string.
	{
		to_tu_string();	// make sure our m_string_value is initialized
		m_type = STRING;
		m_string_value += str;
	}

	void	as_value::drop_refs()
	// Drop any ref counts we have; this happens prior to changing our value.
	{
		if (m_type == AS_FUNCTION)
		{
			if (m_as_function_value)
			{
				m_as_function_value->drop_ref();
				m_as_function_value = 0;
			}
		}
		else if (m_type == OBJECT)
		{
			if (m_object_value)
			{
				m_object_value->drop_ref();
				m_object_value = 0;
			}
		}
	}


	//
	// as_environment
	//


	as_value	as_environment::get_variable(const tu_string& varname, const array<with_stack_entry>& with_stack) const
	// Return the value of the given var, if it's defined.
	{
		// Path lookup rigamarole.
		tu_string	path;
		tu_string	var;
		if (parse_path(varname, &path, &var))
		{
			// [DICE-MOD SwiffPlayer: resolve the path through objects too, not only sprite
			// children - see find_object_path. The shipped get_variable (sub_10091EF0) yields the
			// NUMBER 0, not undefined, when the path still fails; reproduced.]
			as_object_interface*	target = find_object_path(path, NULL);	// @@ Use with_stack here too???  Need to test.
			if (target)
			{
				as_value	val;
				target->get_member(var, &val);
				return val;
			}
			else
			{
				log_error("find_target(\"%s\") failed\n", path.c_str());
				return as_value(0.0);
			}
		}
		else
		{
			return this->get_variable_raw(varname, with_stack);
		}
	}


	as_value	as_environment::get_variable_raw(
		const tu_string& varname,
		const array<with_stack_entry>& with_stack) const
	// varname must be a plain variable name; no path parsing.
	{

		assert(strchr(varname.c_str(), ':') == NULL);
		assert(strchr(varname.c_str(), '/') == NULL);
		assert(strchr(varname.c_str(), '.') == NULL);

		as_value	val;

		// Check the with-stack.
		for (int i = with_stack.size() - 1; i >= 0; i--)
		{
			as_object_interface*	obj = with_stack[i].m_object.get_ptr();
			if (obj && obj->get_member(varname, &val))
			{
				// Found the var in this context.
				return val;
			}
		}

		// Check locals.
		int	local_index = find_local(varname);
		if (local_index >= 0)
		{
			// Get local var.
			return m_local_frames[local_index].m_value;
		}

		// Looking for "this"?
		if (varname == "this")
		{
			// [DICE-MOD: the bound this-object of the running function, else the target movie]
			val.set_as_object_interface(m_this_object ? m_this_object : static_cast<as_object_interface*>(m_target));
			return val;
		}

		// [DICE-MOD SwiffPlayer: `super` is the running function's superclass constructor (the
		// shipped env slot +8216); undefined outside a class constructor.]
		if (varname == "super")
		{
			return m_super_ctor ? as_value(m_super_ctor) : as_value();
		}

		// [DICE-MOD SwiffPlayer: the bound this-object's members come before the target movie's -
		// the shipped get_variable_raw (sub_100914C0) consults env+8212 first.]
		if (m_this_object && m_this_object != static_cast<as_object_interface*>(m_target)
		    && m_this_object->get_member(varname, &val))
		{
			return val;
		}

		// Check movie members.
		if (m_target->get_member(varname, &val))
		{
			return val;
		}

		// Check built-in constants.
		if (varname == "_root" || varname == "_level0")
		{
			return as_value(m_target->get_root_movie());
		}
		if (varname == "_global")
		{
			return as_value(s_global.get_ptr());
		}
		if (s_global->get_member(varname, &val))
		{
			return val;
		}
	
		// Fallback.
		IF_VERBOSE_ACTION(log_msg("get_variable_raw(\"%s\") failed, returning UNDEFINED.\n", varname.c_str()));
		return as_value();
	}


	void	as_environment::set_variable(
		const tu_string& varname,
		const as_value& val,
		const array<with_stack_entry>& with_stack)
	// Given a path to variable, set its value.
	{
		IF_VERBOSE_ACTION(log_msg("-------------- %s = %s\n", varname.c_str(), val.to_string()));//xxxxxxxxxx

		// Path lookup rigamarole.
		tu_string	path;
		tu_string	var;
		if (parse_path(varname, &path, &var))
		{
			// [DICE-MOD SwiffPlayer: through objects too - see find_object_path.]
			as_object_interface*	target = find_object_path(path, NULL);
			if (target)
			{
				target->set_member(var, val);
			}
		}
		else
		{
			this->set_variable_raw(varname, val, with_stack);
		}
	}


	void	as_environment::set_variable_raw(
		const tu_string& varname,
		const as_value& val,
		const array<with_stack_entry>& with_stack)
	// No path rigamarole.
	{
		// Check the with-stack.
		for (int i = with_stack.size() - 1; i >= 0; i--)
		{
			as_object_interface*	obj = with_stack[i].m_object.get_ptr();
			as_value	dummy;
			if (obj && obj->get_member(varname, &dummy))
			{
				// This object has the member; so set it here.
				obj->set_member(varname, val);
				return;
			}
		}

		// Check locals.
		int	local_index = find_local(varname);
		if (local_index >= 0)
		{
			// Set local var.
			m_local_frames[local_index].m_value = val;
			return;
		}

		assert(m_target);

		m_target->set_member(varname, val);
	}


	// [DICE-MOD SwiffPlayer #41] A `var` at TIMELINE level (a frame script, no function frame active)
	// is a property of the clip in Flash - later code reads it through `this`, including functions
	// the same script defines. Stock 2006 pushed it onto the environment's local stack, below the
	// barrier every function call adds, so a function of the same clip never saw it:
	// mainMenu.swf's login page declares `var subPage = Logic.getStorageString("loginSubPage",
	// "unselected")` in its frame script, and its selectProfile() (the account list's callback)
	// tests `subPage != "unselected"` first - it read undefined and returned, so the page never
	// jumped to its "online"/"offline" state (retail run AE). Inside a function frame nothing
	// changes.
	bool	as_environment::in_function_frame() const
	{
		for (int i = m_local_frames.size() - 1; i >= 0; i--)
		{
			const frame_slot&	slot = m_local_frames[i];
			if (slot.m_name.length() == 0 && slot.m_value.get_type() == as_value::BOOLEAN) return true;	// a function barrier
		}
		return false;
	}

	void	as_environment::set_local(const tu_string& varname, const as_value& val)
	// Set/initialize the value of the local variable.
	{
		if (!in_function_frame() && m_target)	// [DICE-MOD SwiffPlayer #41] timeline `var`
		{
			m_target->set_member(varname, val);
			return;
		}
		// Is it in the current frame already?
		int	index = find_local(varname);
		if (index < 0)
		{
			// Not in frame; create a new local var.

			assert(varname.length() > 0);	// null varnames are invalid!
    			m_local_frames.push_back(frame_slot(varname, val));
		}
		else
		{
			// In frame already; modify existing var.
			m_local_frames[index].m_value = val;
		}
	}

	
	void	as_environment::add_local(const tu_string& varname, const as_value& val)
	// Add a local var with the given name and value to our
	// current local frame.  Use this when you know the var
	// doesn't exist yet, since it's faster than set_local();
	// e.g. when setting up args for a function.
	{
		assert(varname.length() > 0);
		m_local_frames.push_back(frame_slot(varname, val));
	}


	void	as_environment::declare_local(const tu_string& varname)
	// Create the specified local var if it doesn't exist already.
	{
		if (!in_function_frame() && m_target)	// [DICE-MOD SwiffPlayer #41] timeline `var x;`
		{
			as_value	existing;
			if (!m_target->get_member(varname, &existing))
			{
				m_target->set_member(varname, as_value());
			}
			return;
		}
		// Is it in the current frame already?
		int	index = find_local(varname);
		if (index < 0)
		{
			// Not in frame; create a new local var.
			assert(varname.length() > 0);	// null varnames are invalid!
    			m_local_frames.push_back(frame_slot(varname, as_value()));
		}
		else
		{
			// In frame already; don't mess with it.
		}
	}

	
	bool	as_environment::get_member(const tu_stringi& varname, as_value* val) const
	{
		return m_variables.get(varname, val);
	}


	void	as_environment::set_member(const tu_stringi& varname, const as_value& val)
	{
		m_variables.set(varname, val);
	}

	as_value*	as_environment::register_ptr(int reg)
	// [DICE-MOD SwiffPlayer #55] The environment's register `reg`. The shipped as_environment holds
	// 256 of them inline (ctor 0x10077D00 constructs 256 undefined values at +0x10) and every
	// register operand is a byte, so any operand names a live slot; the bank is allocated here on
	// first use instead, which reads the same (an untouched register is undefined). Numbering is
	// direct from r0 - the zero-based numbering #43 introduced for the old per-call stack.
	// [DICE-MOD SwiffPlayer #57] ...in the bank register_bank() names: the calling chain's while one
	// of this environment's functions runs for another environment - see as_as_function::operator().
	{
		as_environment*	bank = register_bank();
		if (bank->m_registers.size() == 0)
		{
			bank->m_registers.resize(256);
		}
		return &bank->m_registers[reg & 0xFF];
	}


	int	as_environment::find_local(const tu_string& varname) const
	// Search the active frame for the named var; return its index
	// in the m_local_frames stack if found.
	// 
	// Otherwise return -1.
	{
		// Linear search sucks, but is probably fine for
		// typical use of local vars in script.  There could
		// be pathological breakdowns if a function has tons
		// of locals though.  The ActionScript bytecode does
		// not help us much by using strings to index locals.

		for (int i = m_local_frames.size() - 1; i >= 0; i--)
		{
			const frame_slot&	slot = m_local_frames[i];
			if (slot.m_name.length() == 0)
			{
				// End of local frame; stop looking.
				return -1;
			}
			else if (slot.m_name == varname)
			{
				// Found it.
				return i;
			}
		}
		return -1;
	}


	bool	as_environment::parse_path(const tu_string& var_path, tu_string* path, tu_string* var) const
	// See if the given variable name is actually a sprite path
	// followed by a variable name.  These come in the format:
	//
	// 	/path/to/some/sprite/:varname
	//
	// (or same thing, without the last '/')
	//
	// or
	//	path.to.some.var
	//
	// If that's the format, puts the path part (no colon or
	// trailing slash) in *path, and the varname part (no color)
	// in *var and returns true.
	//
	// If no colon, returns false and leaves *path & *var alone.
	{
		// Search for colon.
		int	colon_index = 0;
		int	var_path_length = var_path.length();
		for ( ; colon_index < var_path_length; colon_index++)
		{
			if (var_path[colon_index] == ':')
			{
				// Found it.
				break;
			}
		}

		if (colon_index >= var_path_length)
		{
			// No colon.  Is there a '.'?  Find the last
			// one, if any.
			for (colon_index = var_path_length - 1; colon_index >= 0; colon_index--)
			{
				if (var_path[colon_index] == '.')
				{
					// Found it.
					break;
				}
			}
			if (colon_index < 0) return false;
		}

		// Make the subparts.

		// Var.
		*var = &var_path[colon_index + 1];

		// Path.
		if (colon_index > 0)
		{
			if (var_path[colon_index - 1] == '/')
			{
				// Trim off the extraneous trailing slash.
				colon_index--;
			}
		}
		// @@ could be better.  This whole usage of tu_string is very flabby...
		*path = var_path;
		path->resize(colon_index);

		return true;
	}


	movie*	as_environment::find_target(const as_value& val) const
	// Find the sprite/movie represented by the given value.  The
	// value might be a reference to the object itself, or a
	// string giving a relative path name to the object.
	{
		if (val.get_type() == as_value::OBJECT)
		{
			if (val.to_object() != NULL)
			{
				return val.to_object()->to_movie();
			}
			else
			{
				return NULL;
			}
		}
		else if (val.get_type() == as_value::STRING)
		{
			return find_target(val.to_tu_string());
		}
		else
		{
			log_error("error: invalid path; neither string nor object");
			return NULL;
		}
	}


	static const char*	next_slash_or_dot(const char* word)
	// Search for next '.' or '/' character in this word.  Return
	// a pointer to it, or to NULL if it wasn't found.
	{
		for (const char* p = word; *p; p++)
		{
			if (*p == '.' && p[1] == '.')
			{
				p++;
			}
			else if (*p == '.' || *p == '/')
			{
				return p;
			}
		}

		return NULL;
	}


	movie*	as_environment::find_target(const tu_string& path) const
	// Find the sprite/movie referenced by the given path.
	{
		if (path.length() <= 0)
		{
			return m_target;
		}

		assert(path.length() > 0);

		movie*	env = m_target;
		assert(env);
		
		const char*	p = path.c_str();
		tu_string	subpart;

		if (*p == '/')
		{
			// Absolute path.  Start at the root.
			env = env->get_relative_target("_level0");
			p++;
		}

		for (;;)
		{
			const char*	next_slash = next_slash_or_dot(p);
			subpart = p;
			if (next_slash == p)
			{
				log_error("error: invalid path '%s'\n", path.c_str());
				break;
			}
			else if (next_slash)
			{
				// Cut off the slash and everything after it.
				subpart.resize(next_slash - p);
			}

			env = env->get_relative_target(subpart);
			//@@   _level0 --> root, .. --> parent, . --> this, other == character

			if (env == NULL || next_slash == NULL)
			{
				break;
			}

			p = next_slash + 1;
		}
		return env;
	}


	// [DICE-MOD SwiffPlayer: dotted-path resolution through OBJECT members.
	//
	// Stock 2006 resolves the path part of "a.b.c" (get_variable/set_variable's "path lookup
	// rigamarole") only through movie::get_relative_target, i.e. sprite CHILDREN; a plain object's
	// member is invisible to it. BF2's menu is an AS2 class framework whose fully qualified class
	// names ("dice.UIs.Commons.Button", "dice.UpdateManager", ...) the compiler emits as dotted
	// GetVariable/SetVariable strings, so on stock gameswf every one of them fails with
	// find_target("dice.UIs.Commons") failed, the framework never builds, and the root movie stays
	// stopped on frame 0 (retail 2026-09-02, ~20 such failures before the first frame).
	//
	// The shipped DICE gameswf resolves them (SwiffPlayer.dll (2).i64):
	//   * as_environment::find_target (sub_10091960) walks each path element through the VIRTUAL
	//     get_relative_target of whatever the current element is (as_object_interface vslot +28);
	//   * as_object::get_relative_target (sub_1000B3B0): "." / "this" -> self; ".." / "_level0" /
	//     "_root" -> null; anything else -> get_member(name).to_object();
	//   * when the walk dies it RE-RESOLVES the whole path from the _global object (unless it
	//     started there) - which is how "dice.*" resolves from a sprite's own script;
	//   * get_variable (sub_10091EF0) yields the NUMBER 0, not undefined, when that still fails.
	// This is that behaviour on the stock 2006 types: a sprite resolves its children first (its own
	// get_relative_target), then its members; a plain object resolves members. See
	// GAMESWF_DIVERGENCES.md.]
	static as_object_interface*	resolve_path_element(as_object_interface* env, const tu_string& name)
	{
		if (env == NULL)
		{
			return NULL;
		}
		movie*	mv = env->to_movie();
		if (mv)
		{
			movie*	child = mv->get_relative_target(name);
			if (child)
			{
				return child;
			}
		}
		else
		{
			if (name == "." || name == "this")
			{
				return env;
			}
			if (name == ".." || name == "_level0" || name == "_root")
			{
				return NULL;
			}
		}
		as_value	val;
		if (env->get_member(name, &val))
		{
			return val.to_object();
		}
		return NULL;
	}

	as_object_interface*	as_environment::find_object_path(const tu_string& path, as_object_interface* start) const
	{
		as_object_interface*	env = start ? start : m_target;
		if (path.length() <= 0)
		{
			return m_target;
		}
		if (env == NULL)
		{
			return NULL;
		}

		const char*	p = path.c_str();
		tu_string	subpart;

		if (*p == '/')
		{
			// Absolute path.  Start at the root.
			movie*	mv = env->to_movie();
			env = mv ? mv->get_relative_target("_level0") : NULL;
			p++;
		}

		for (;;)
		{
			const char*	next_slash = next_slash_or_dot(p);
			subpart = p;
			if (next_slash == p)
			{
				log_error("error: invalid path '%s'\n", path.c_str());
				break;
			}
			else if (next_slash)
			{
				// Cut off the slash and everything after it.
				subpart.resize(next_slash - p);
			}

			env = resolve_path_element(env, subpart);

			if (env == NULL || next_slash == NULL)
			{
				break;
			}

			p = next_slash + 1;
		}

		if (env == NULL)
		{
			// The shipped fallback: resolve the whole path again from _global, unless that is
			// where this walk started.
			as_object_interface*	global = s_global.get_ptr();
			if (global && start != global)
			{
				return find_object_path(path, global);
			}
		}
		return env;
	}


	//
	// event_id
	//

	// [DICE-MOD SwiffPlayer #51] the shipped table (sub_1008BA90) spells onReleaseOutside, onRollOver,
	// onRollOut, onDragOver and onDragOut without stock 2006's underscores - a sprite's on_event looks
	// its member handler up by this name, so the stock spellings never found one - and ends in
	// onDoublePress.
	const tu_string&	event_id::get_function_name() const
	{
		static tu_string	s_function_names[EVENT_COUNT] =
		{
			"INVALID",		 // INVALID
			"onPress",		 // PRESS
			"onRelease",		 // RELEASE
			"onReleaseOutside",	 // RELEASE_OUTSIDE
			"onRollOver",		 // ROLL_OVER
			"onRollOut",		 // ROLL_OUT
			"onDragOver",		 // DRAG_OVER
			"onDragOut",		 // DRAG_OUT
			"onKeyPress",		 // KEY_PRESS
			"onInitialize",		 // INITIALIZE

			"onLoad",		 // LOAD
			"onUnload",		 // UNLOAD
			"onEnterFrame",		 // ENTER_FRAME
			"onMouseDown",		 // MOUSE_DOWN
			"onMouseUp",		 // MOUSE_UP
			"onMouseMove",		 // MOUSE_MOVE
			"onKeyDown",		 // KEY_DOWN
			"onKeyUp",		 // KEY_UP
			"onData",		 // DATA
			// These are for the MoveClipLoader ActionScript only
			"onLoadStart",		 // LOAD_START
			"onLoadError",		 // LOAD_ERROR
			"onLoadProgress",	 // LOAD_PROGRESS
			"onLoadInit",		 // LOAD_INIT
			// These are for the XMLSocket ActionScript only
			"onSockClose",		 // CLOSE
			"onSockConnect",	 // CONNECT
			"onSockXML",		 // XML
			// These are for the XML ActionScript only
			"onXMLLoad",		 // XML_LOAD
			"onXMLData",		 // XML_DATA
			"onTimer",	         // setInterval Timer expired
			"onConstruct",		 // CONSTRUCT [DICE-MOD SwiffPlayer #27]
			"onDoublePress",	 // DOUBLE_PRESS [DICE-MOD SwiffPlayer #51]
		};

		assert(m_id > INVALID && m_id < EVENT_COUNT);
		return s_function_names[m_id];
	}


	// Standard member lookup.
	as_standard_member	get_standard_member(const tu_stringi& name)
	{
		static bool	s_inited = false;
		static stringi_hash<as_standard_member>	s_standard_member_map;
		if (!s_inited)
		{
			s_inited = true;

			s_standard_member_map.set_capacity(int(AS_STANDARD_MEMBER_COUNT));

			s_standard_member_map.add("_x", M_X);
			s_standard_member_map.add("_y", M_Y);
			s_standard_member_map.add("_xscale", M_XSCALE);
			s_standard_member_map.add("_yscale", M_YSCALE);
			s_standard_member_map.add("_currentframe", M_CURRENTFRAME);
			s_standard_member_map.add("_totalframes", M_TOTALFRAMES);
			s_standard_member_map.add("_alpha", M_ALPHA);
			s_standard_member_map.add("_visible", M_VISIBLE);
			s_standard_member_map.add("_width", M_WIDTH);
			s_standard_member_map.add("_height", M_HEIGHT);
			s_standard_member_map.add("_rotation", M_ROTATION);
			s_standard_member_map.add("_target", M_TARGET);
			s_standard_member_map.add("_framesloaded", M_FRAMESLOADED);
			s_standard_member_map.add("_name", M_NAME);
			s_standard_member_map.add("_droptarget", M_DROPTARGET);
			s_standard_member_map.add("_url", M_URL);
			s_standard_member_map.add("_highquality", M_HIGHQUALITY);
			s_standard_member_map.add("_focusrect", M_FOCUSRECT);
			s_standard_member_map.add("_soundbuftime", M_SOUNDBUFTIME);
			s_standard_member_map.add("_xmouse", M_XMOUSE);
			s_standard_member_map.add("_ymouse", M_YMOUSE);
			s_standard_member_map.add("_parent", M_PARENT);
			s_standard_member_map.add("text", M_TEXT);
			s_standard_member_map.add("textWidth", M_TEXTWIDTH);
			s_standard_member_map.add("textColor", M_TEXTCOLOR);
			s_standard_member_map.add("onLoad", M_ONLOAD);
		}

		as_standard_member	result = M_INVALID_MEMBER;
		s_standard_member_map.get(name, &result);

		return result;
	}


	//
	// Disassembler
	//


#define COMPILE_DISASM 1

#ifndef COMPILE_DISASM

	void	log_disasm(const unsigned char* instruction_data)
	// No disassembler in this version...
	{
		log_msg("<no disasm>\n");
	}

#else // COMPILE_DISASM

	void	log_disasm(const unsigned char* instruction_data)
	// Disassemble one instruction to the log.
	{
		enum arg_format {
			ARG_NONE = 0,
			ARG_STR,
			ARG_HEX,	// default hex dump, in case the format is unknown or unsupported
			ARG_U8,
			ARG_U16,
			ARG_S16,
			ARG_PUSH_DATA,
			ARG_DECL_DICT,
			ARG_FUNCTION2
		};
		struct inst_info
		{
			int	m_action_id;
			const char*	m_instruction;

			arg_format	m_arg_format;
		};

		static inst_info	s_instruction_table[] = {
			{ 0x04, "next_frame", ARG_NONE },
			{ 0x05, "prev_frame", ARG_NONE },
			{ 0x06, "play", ARG_NONE },
			{ 0x07, "stop", ARG_NONE },
			{ 0x08, "toggle_qlty", ARG_NONE },
			{ 0x09, "stop_sounds", ARG_NONE },
			{ 0x0A, "add", ARG_NONE },
			{ 0x0B, "sub", ARG_NONE },
			{ 0x0C, "mul", ARG_NONE },
			{ 0x0D, "div", ARG_NONE },
			{ 0x0E, "equ", ARG_NONE },
			{ 0x0F, "lt", ARG_NONE },
			{ 0x10, "and", ARG_NONE },
			{ 0x11, "or", ARG_NONE },
			{ 0x12, "not", ARG_NONE },
			{ 0x13, "str_eq", ARG_NONE },
			{ 0x14, "str_len", ARG_NONE },
			{ 0x15, "substr", ARG_NONE },
			{ 0x17, "pop", ARG_NONE },
			{ 0x18, "floor", ARG_NONE },
			{ 0x1C, "get_var", ARG_NONE },
			{ 0x1D, "set_var", ARG_NONE },
			{ 0x20, "set_target_exp", ARG_NONE },
			{ 0x21, "str_cat", ARG_NONE },
			{ 0x22, "get_prop", ARG_NONE },
			{ 0x23, "set_prop", ARG_NONE },
			{ 0x24, "dup_sprite", ARG_NONE },
			{ 0x25, "rem_sprite", ARG_NONE },
			{ 0x26, "trace", ARG_NONE },
			{ 0x27, "start_drag", ARG_NONE },
			{ 0x28, "stop_drag", ARG_NONE },
			{ 0x29, "str_lt", ARG_NONE },
			{ 0x2B, "cast_object", ARG_NONE },
			{ 0x30, "random", ARG_NONE },
			{ 0x31, "mb_length", ARG_NONE },
			{ 0x32, "ord", ARG_NONE },
			{ 0x33, "chr", ARG_NONE },
			{ 0x34, "get_timer", ARG_NONE },
			{ 0x35, "substr_mb", ARG_NONE },
			{ 0x36, "ord_mb", ARG_NONE },
			{ 0x37, "chr_mb", ARG_NONE },
			{ 0x3A, "delete", ARG_NONE },
			{ 0x3B, "delete_all", ARG_NONE },
			{ 0x3C, "set_local", ARG_NONE },
			{ 0x3D, "call_func", ARG_NONE },
			{ 0x3E, "return", ARG_NONE },
			{ 0x3F, "mod", ARG_NONE },
			{ 0x40, "new", ARG_NONE },
			{ 0x41, "decl_local", ARG_NONE },
			{ 0x42, "decl_array", ARG_NONE },
			{ 0x43, "decl_obj", ARG_NONE },
			{ 0x44, "type_of", ARG_NONE },
			{ 0x45, "get_target", ARG_NONE },
			{ 0x46, "enumerate", ARG_NONE },
			{ 0x47, "add_t", ARG_NONE },
			{ 0x48, "lt_t", ARG_NONE },
			{ 0x49, "eq_t", ARG_NONE },
			{ 0x4A, "number", ARG_NONE },
			{ 0x4B, "string", ARG_NONE },
			{ 0x4C, "dup", ARG_NONE },
			{ 0x4D, "swap", ARG_NONE },
			{ 0x4E, "get_member", ARG_NONE },
			{ 0x4F, "set_member", ARG_NONE },
			{ 0x50, "inc", ARG_NONE },
			{ 0x51, "dec", ARG_NONE },
			{ 0x52, "call_method", ARG_NONE },
			{ 0x53, "new_method", ARG_NONE },
			{ 0x54, "is_inst_of", ARG_NONE },
			{ 0x55, "enum_object", ARG_NONE },
			{ 0x60, "bit_and", ARG_NONE },
			{ 0x61, "bit_or", ARG_NONE },
			{ 0x62, "bit_xor", ARG_NONE },
			{ 0x63, "shl", ARG_NONE },
			{ 0x64, "asr", ARG_NONE },
			{ 0x65, "lsr", ARG_NONE },
			{ 0x66, "eq_strict", ARG_NONE },
			{ 0x67, "gt_t", ARG_NONE },
			{ 0x68, "gt_str", ARG_NONE },
			
			{ 0x81, "goto_frame", ARG_U16 },
			{ 0x83, "get_url", ARG_STR },
			{ 0x87, "store_register", ARG_U8 },
			{ 0x88, "decl_dict", ARG_DECL_DICT },
			{ 0x8A, "wait_for_frame", ARG_HEX },
			{ 0x8B, "set_target", ARG_STR },
			{ 0x8C, "goto_frame_lbl", ARG_STR },
			{ 0x8D, "wait_for_fr_exp", ARG_HEX },
			{ 0x8E, "function2", ARG_FUNCTION2 },
			{ 0x94, "with", ARG_U16 },
			{ 0x96, "push_data", ARG_PUSH_DATA },
			{ 0x99, "goto", ARG_S16 },
			{ 0x9A, "get_url2", ARG_HEX },
			// { 0x8E, "function2", ARG_HEX },
			{ 0x9B, "func", ARG_HEX },
			{ 0x9D, "branch_if_true", ARG_S16 },
			{ 0x9E, "call_frame", ARG_HEX },
			{ 0x9F, "goto_frame_exp", ARG_HEX },
			{ 0x00, "<end>", ARG_NONE }
		};

		int	action_id = instruction_data[0];
		inst_info*	info = NULL;

		for (int i = 0; ; i++)
		{
			if (s_instruction_table[i].m_action_id == action_id)
			{
				info = &s_instruction_table[i];
			}

			if (s_instruction_table[i].m_action_id == 0)
			{
				// Stop at the end of the table and give up.
				break;
			}
		}

		arg_format	fmt = ARG_HEX;

		// Show instruction.
		if (info == NULL)
		{
			log_msg("<unknown>[0x%02X]", action_id);
		}
		else
		{
			log_msg("%-15s", info->m_instruction);
			fmt = info->m_arg_format;
		}

		// Show instruction argument(s).
		if (action_id & 0x80)
		{
			assert(fmt != ARG_NONE);

			int	length = instruction_data[1] | (instruction_data[2] << 8);

			// log_msg(" [%d]", length);

			if (fmt == ARG_HEX)
			{
				for (int i = 0; i < length; i++)
				{
					log_msg(" 0x%02X", instruction_data[3 + i]);
				}
				log_msg("\n");
			}
			else if (fmt == ARG_STR)
			{
				log_msg(" \"");
				for (int i = 0; i < length; i++)
				{
					log_msg("%c", instruction_data[3 + i]);
				}
				log_msg("\"\n");
			}
			else if (fmt == ARG_U8)
			{
				int	val = instruction_data[3];
				log_msg(" %d\n", val);
			}
			else if (fmt == ARG_U16)
			{
				int	val = instruction_data[3] | (instruction_data[4] << 8);
				log_msg(" %d\n", val);
			}
			else if (fmt == ARG_S16)
			{
				int	val = instruction_data[3] | (instruction_data[4] << 8);
				if (val & 0x8000) val |= ~0x7FFF;	// sign-extend
				log_msg(" %d\n", val);
			}
			else if (fmt == ARG_PUSH_DATA)
			{
				log_msg("\n");
				int i = 0;
				while (i < length)
				{
					int	type = instruction_data[3 + i];
					i++;
					log_msg("\t\t");	// indent
					if (type == 0)
					{
						// string
						log_msg("\"");
						while (instruction_data[3 + i])
						{
							log_msg("%c", instruction_data[3 + i]);
							i++;
						}
						i++;
						log_msg("\"\n");
					}
					else if (type == 1)
					{
						// float (little-endian)
						union {
							float	f;
							Uint32	i;
						} u;
						compiler_assert(sizeof(u) == sizeof(u.i));

						memcpy(&u.i, instruction_data + 3 + i, 4);
						u.i = swap_le32(u.i);
						i += 4;

						log_msg("(float) %f\n", u.f);
					}
					else if (type == 2)
					{
						log_msg("NULL\n");
					}
					else if (type == 3)
					{
						log_msg("undef\n");
					}
					else if (type == 4)
					{
						// contents of register
						int	reg = instruction_data[3 + i];
						i++;
						log_msg("reg[%d]\n", reg);
					}
					else if (type == 5)
					{
						int	bool_val = instruction_data[3 + i];
						i++;
						log_msg("bool(%d)\n", bool_val);
					}
					else if (type == 6)
					{
						// double
						// wacky format: 45670123
						union {
							double	d;
							Uint64	i;
							struct {
								Uint32	lo;
								Uint32	hi;
							} sub;
						} u;
						compiler_assert(sizeof(u) == sizeof(u.i));

						memcpy(&u.sub.hi, instruction_data + 3 + i, 4);
						memcpy(&u.sub.lo, instruction_data + 3 + i + 4, 4);
						u.i = swap_le64(u.i);
						i += 8;

						log_msg("(double) %f\n", u.d);
					}
					else if (type == 7)
					{
						// int32
						Sint32	val = instruction_data[3 + i]
							| (instruction_data[3 + i + 1] << 8)
							| (instruction_data[3 + i + 2] << 16)
							| (instruction_data[3 + i + 3] << 24);
						i += 4;
						log_msg("(int) %d\n", val);
					}
					else if (type == 8)
					{
						int	id = instruction_data[3 + i];
						i++;
						log_msg("dict_lookup[%d]\n", id);
					}
					else if (type == 9)
					{
						int	id = instruction_data[3 + i] | (instruction_data[3 + i + 1] << 8);
						i += 2;
						log_msg("dict_lookup_lg[%d]\n", id);
					}
				}
			}
			else if (fmt == ARG_DECL_DICT)
			{
				int	i = 0;
				int	count = instruction_data[3 + i] | (instruction_data[3 + i + 1] << 8);
				i += 2;

				log_msg(" [%d]\n", count);

				// Print strings.
				for (int ct = 0; ct < count; ct++)
				{
					log_msg("\t\t");	// indent

					log_msg("\"");
					while (instruction_data[3 + i])
					{
						// safety check.
						if (i >= length)
						{
							log_msg("<disasm error -- length exceeded>\n");
							break;
						}

						log_msg("%c", instruction_data[3 + i]);
						i++;
					}
					log_msg("\"\n");
					i++;
				}
			}
			else if (fmt == ARG_FUNCTION2)
			{
				// Signature info for a function2 opcode.
				int	i = 0;
				const char*	function_name = (const char*) &instruction_data[3 + i];
				i += strlen(function_name) + 1;

				int	arg_count = instruction_data[3 + i] | (instruction_data[3 + i + 1] << 8);
				i += 2;

				int	reg_count = instruction_data[3 + i];
				i++;

				log_msg("\n\t\tname = '%s', arg_count = %d, reg_count = %d\n",
					function_name, arg_count, reg_count);

				uint16	flags = (instruction_data[3 + i]) | (instruction_data[3 + i + 1] << 8);
				i += 2;

				// @@ What is the difference between "super" and "_parent"?

				bool	preload_global = (flags & 0x100) != 0;
				bool	preload_parent = (flags & 0x80) != 0;
				bool	preload_root   = (flags & 0x40) != 0;
				bool	suppress_super = (flags & 0x20) != 0;
				bool	preload_super  = (flags & 0x10) != 0;
				bool	suppress_args  = (flags & 0x08) != 0;
				bool	preload_args   = (flags & 0x04) != 0;
				bool	suppress_this  = (flags & 0x02) != 0;
				bool	preload_this   = (flags & 0x01) != 0;

				log_msg("\t\t        pg = %d\n"
					"\t\t        pp = %d\n"
					"\t\t        pr = %d\n"
					"\t\tss = %d, ps = %d\n"
					"\t\tsa = %d, pa = %d\n"
					"\t\tst = %d, pt = %d\n",
					int(preload_global),
					int(preload_parent),
					int(preload_root),
					int(suppress_super),
					int(preload_super),
					int(suppress_args),
					int(preload_args),
					int(suppress_this),
					int(preload_this));

				for (int argi = 0; argi < arg_count; argi++)
				{
					int	arg_register = instruction_data[3 + i];
					i++;
					const char*	arg_name = (const char*) &instruction_data[3 + i];
					i += strlen(arg_name) + 1;

					log_msg("\t\targ[%d] - reg[%d] - '%s'\n", argi, arg_register, arg_name);
				}

				int	function_length = instruction_data[3 + i] | (instruction_data[3 + i + 1] << 8);
				i += 2;

				log_msg("\t\tfunction length = %d\n", function_length);
			}
		}
		else
		{
			log_msg("\n");
		}
	}

#endif // COMPILE_DISASM


};


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
