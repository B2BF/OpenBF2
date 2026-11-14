// gameswf_action.cpp	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Implementation and helpers for SWF actions.


// [CHOSEN, not recovered] issue #7: the script-object registry. Included before the gameswf headers:
// MSVC's <unordered_set> pulls in <cmath>, whose isfinite template the fake isfinite() macro of
// base/tu_math.h would otherwise rename.
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include "../../Platform/X87Compat.h"
#include "gameswf_action.h"
#include "gameswf_impl.h"
#include "gameswf_log.h"
#include "gameswf_profile.h"
#include "gameswf_stream.h"
#include "base/tu_random.h"

#if defined(_WIN32)
extern "C" __declspec(dllimport) short __stdcall GetAsyncKeyState(int vkey);
#endif

#include "gameswf_string.h"
#include "gameswf_timers.h"
#include "gameswf_sound.h"
#include "gameswf_xml.h"



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

	static void number_value_of(const fn_call& fn);
	static void number_to_string(const fn_call& fn);
	static void boolean_value_of(const fn_call& fn);
	static void boolean_to_string(const fn_call& fn);
	// SwiffPlayer.dll 0x100F982C; source-level name [INFERRED].
	static Uint32 s_object_live_count = 0;

	struct number_object : public as_object
	{
		double	m_value;
		number_object()
		{
			// SwiffPlayer.dll 0x100A9AD0: toString before valueOf.
			// The numeric field stays unwritten until the factory receives an argument.
			set_member("toString", &number_to_string);
			set_member("valueOf", &number_value_of);
		}
		// SwiffPlayer.dll 0x100A9BE0: only base destruction before deletion.
		~number_object() override = default;
		// SwiffPlayer.dll 0x100A99C0: direct concrete forwarder.
		void set_member(const tu_stringi& name, const as_value& value) override
		{
			as_object::set_member(name, value);
		}
		// SwiffPlayer.dll 0x100A99D0: direct concrete forwarder.
		bool get_member(const tu_stringi& name, as_value* value) override
		{
			return as_object::get_member(name, value);
		}
		virtual const char* get_text_value() const
		{
			// SwiffPlayer.dll 0x100A99E0: every Number shares one %f buffer.
			// [CHOSEN, not recovered] S3 bridge: native buffer capacity and CRT.
			static char text[512];
			sprintf(text, "%f", m_value);
			return text;
		}
	};
	struct boolean_object : public as_object
	{
		bool	m_value;
		boolean_object() : m_value(false)
		{
			// SwiffPlayer.dll 0x100A9880: false, then valueOf before toString.
			set_member("valueOf", &boolean_value_of);
			set_member("toString", &boolean_to_string);
		}
		// SwiffPlayer.dll 0x100A9990: only base destruction before deletion.
		~boolean_object() override = default;
		// SwiffPlayer.dll vtable 0x100E3698 +0x18 -> 0x1000B390 returns null.
		// Inherit as_object's null text slot; use toString for Boolean text.
	};

	static void number_value_of(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100A9A10 reads before releasing the result's old value.
		const double value = static_cast<number_object*>(fn.this_ptr)->m_value;
		fn.result->set_double(value);
	}

	static void number_to_string(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100A9A40 constructs before reading the receiver.
		as_object_interface* text = create_dice_string_object(NULL, 0);
		const as_value value(static_cast<number_object*>(fn.this_ptr)->m_value);
		assign_dice_string_object_narrow(text, value.to_string());
		fn.result->set_as_object_interface(text);
	}

	static void boolean_value_of(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100A97E0: Boolean result, not a numeric result.
		const bool value = static_cast<boolean_object*>(fn.this_ptr)->m_value;
		fn.result->set_bool(value);
	}

	static void boolean_to_string(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100A9820 constructs before reading the receiver.
		as_object_interface* text = create_dice_string_object(NULL, 0);
		assign_dice_string_object_narrow(text,
			static_cast<boolean_object*>(fn.this_ptr)->m_value ? "true" : "false");
		fn.result->set_as_object_interface(text);
	}

	void	as_global_number_ctor(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008C520 registers methods before argument conversion.
		number_object*	obj = new number_object;
		if (fn.nargs != 0) obj->m_value = fn.arg(0).to_number();
		fn.result->set_as_object_interface(obj);
	}
	void	as_global_boolean_ctor(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008C4D0 registers methods before argument conversion.
		boolean_object*	obj = new boolean_object;
		if (fn.nargs != 0) obj->m_value = fn.arg(0).to_bool();
		fn.result->set_as_object_interface(obj);
	}

	void	action_init();
	// Native factory for the separately published retail String class object.
	as_object* create_dice_string_class();

	// Statics.
	bool	s_inited = false;
	smart_ptr<as_object>	s_global;
	// SwiffPlayer.dll 0x100F94CC: modules are separate from the global object's members.
	// [CHOSEN, not recovered] S3 bridge: module values use the native object bags.
	static stringi_hash<as_value> s_modules;
	// SwiffPlayer.dll 0x100F9358 / initialized release sentinel 0x100ECE68.
	static Uint32 s_gc_mark_generation = 0;
	Uint32 get_gc_mark_generation() { return s_gc_mark_generation; }
	Uint32 get_gc_release_generation() { return 0xFEEDBEEFu; }
	void advance_gc_mark_generation() { ++s_gc_mark_generation; }

	void gc_mark_value(const as_value& value)
	{
		// SwiffPlayer.dll 0x1000B520: functions and objects take distinct paths.
		if (value.get_type() == as_value::AS_FUNCTION)
		{
			if (as_as_function* function = value.to_as_function()) function->gc_mark();
		}
		else if (value.get_type() == as_value::OBJECT)
		{
			if (as_object_interface* object = value.to_object()) object->gc_mark();
		}
	}

	void gc_mark_modules()
	{
		// SwiffPlayer.dll 0x1008C570 advances twice after a non-null function.
		// Do not merge this branch into gc_mark_value and lose the skipped entry.
		for (stringi_hash<as_value>::iterator it = s_modules.begin(); it != s_modules.end(); ++it)
		{
			if (it->second.get_type() == as_value::AS_FUNCTION && it->second.to_as_function())
			{
				it->second.to_as_function()->gc_mark();
				++it;
			}
			else if (it->second.get_type() == as_value::OBJECT && it->second.to_object())
			{
				it->second.to_object()->gc_mark();
			}
		}
	}
	// SwiffPlayer.dll 0x100D6500 registers this table's exit destructor;
	// 0x100D6E60 destroys its entries/storage. NewObject factories at 0x100F94C8
	// are independent of modules
	// and writable _global members (registration 0x1009124C-0x100913F5).
	static stringi_hash<as_c_function_ptr> s_constructors;
	fscommand_callback	s_fscommand_handler = NULL;
	// SwiffPlayer.dll 0x100925B4/0x100925C1 publishes the currently executing environment.
	static as_environment* s_current_environment = NULL;
	// SwiffPlayer.dll 0x100F9828: member lookup supplies the next call's superclass.
	static as_as_function* s_call_super = NULL;
	void set_call_super(as_as_function* constructor) { s_call_super = constructor; }
	void set_environment_super(as_environment* env) { env->m_super_ctor = s_call_super; }

	static as_object_interface* lookup_global_object()
	{
		// SwiffPlayer.dll 0x1008E4D0: lookup only; no implicit initialization.
		as_value global;
		if (!s_modules.get("_global", &global)) return NULL;
		return global.get_type() == as_value::OBJECT ? global.m_object_value : NULL;
	}

	static void set_global_member(const char* name, const as_value& value)
	{
		// SwiffPlayer.dll 0x1008E5E0: a missing module skips name construction;
		// a present non-object module still reaches the unguarded virtual call.
		as_value global;
		if (s_modules.get("_global", &global))
		{
			as_object_interface* object = global.get_type() == as_value::OBJECT
				? global.m_object_value : NULL;
			const tu_stringi member(name);
			object->set_member(member, value);
		}
	}

	static void register_global_function(const char* name, as_c_function_ptr function)
	{
		// SwiffPlayer.dll 0x1008E6E0: lookup, name, function value, virtual write.
		as_value global;
		if (s_modules.get("_global", &global))
		{
			as_object_interface* object = global.get_type() == as_value::OBJECT
				? global.m_object_value : NULL;
			const tu_stringi member(name);
			const as_value value(function);
			object->set_member(member, value);
		}
	}

	static void register_constructor(const char* name, as_c_function_ptr constructor)
	{
		// SwiffPlayer.dll 0x1008E830: add to the private factory table.
		const tu_stringi key(name);
		s_constructors.add(key, constructor);
	}


	void	register_fscommand_callback(fscommand_callback handler)
	// External interface.
	{
		// SwiffPlayer.dll 0x1008A1A0 replaces the callback, including NULL.
		s_fscommand_handler = handler;
	}



	//
	// array object
	//


	// Array methods are registered on each instance; indexed values have separate dense storage.
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

	// SwiffPlayer.dll 0x10099CB0: virtual prototype enumeration precedes own
	// members, filtered on the shipped value types 1 through 4.
	// [CHOSEN, not recovered] S3 bridge: the retained 2006 NULLTYPE passes too;
	// retail Push null instead constructs a STRING "NULL" (0x10094F66).
	void	as_object::enumerate(as_environment* env)
	{
		if (m_prototype)
		{
			m_prototype->enumerate(env);
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

	// Destructor body SwiffPlayer.dll 0x1007CDB0 (called by 0x1009D1C0): member
	// hash at +0x38, then weak-proxy release. [CHOSEN, not recovered] issue #7:
	// native member containers and weak proxies perform the same releases.
	struct as_array_object : public as_object
	{
		// Compiler deleting wrapper: SwiffPlayer.dll 0x1009D1C0; it delegates
		// to the Array lifetime body before conditional deletion.
		// 0x10077F50 destroys dense values before the object base's live-count
		// decrement, accessor keys, constructor and ordinary members.
		~as_array_object() override = default;
		virtual void gc_mark()
		{
			// SwiffPlayer.dll 0x1009CF50 marks the inherited object first, then
			// only OBJECT elements. Functions in dense storage are not visited.
			if (m_gc_generation == get_gc_mark_generation()) return;
			as_object::gc_mark();
			for (int i = 0; i < m_elements.size(); ++i)
				if (m_elements[i].get_type() == as_value::OBJECT && m_elements[i].to_object())
					m_elements[i].to_object()->gc_mark();
		}
		virtual void gc_release()
		{
			// SwiffPlayer.dll 0x1009CFC0: inherited members before OBJECT
			// elements, then clear dense storage. No function traversal.
			if (m_gc_generation == get_gc_release_generation()) return;
			as_object::gc_release();
			for (int i = 0; i < m_elements.size(); ++i)
				if (m_elements[i].get_type() == as_value::OBJECT && m_elements[i].to_object())
					m_elements[i].to_object()->gc_release();
			m_elements.clear();
		}

		// [CHOSEN, not recovered] S3 bridge: retain the native as_object interface and
		// ownership policy. Indexed storage and the Array operations below follow retail.
		array<as_value>	m_elements;

		as_array_object()
		{
			// SwiffPlayer.dll 0x1009CB20: preserve builtin registration order.
			this->set_member("join", &array_join);
			this->set_member("concat", &array_concat);
			this->set_member("reverse", &array_reverse);
			this->set_member("toString", &array_to_string);
			this->set_member("pop", &array_pop);
			this->set_member("push", &array_push);
			this->set_member("shift", &array_shift);
			this->set_member("slice", &array_slice);
			this->set_member("sort", &array_sort);
			this->set_member("splice", &array_splice);
			this->set_member("unshift", &array_unshift);
		}

		// SwiffPlayer.dll 0x1009D020 enumerates only indices, including undefined holes.
		virtual void	enumerate(as_environment* env)
		{
			for (int i = 0; i < m_elements.size(); i++)
			{
				char	buf[16];
				sprintf(buf, "%d", i);
				env->push(as_value(buf));
			}
		}

		const as_value&	get_element(int i) const { return m_elements[i]; }

		void	set_element(int i, const as_value& v)
		{
			if (i >= m_elements.size()) m_elements.resize(i + 1);
			m_elements[i] = v;
		}

		virtual bool	get_member(const tu_stringi& name, as_value* val)
		{
			if (strcmp(name.c_str(), "length") == 0)
			{
				// 0x1009C215/0x1009C21C release the result twice before writing it.
				const int size = m_elements.size();
				val->set_int(size);
				return true;
			}
			// SwiffPlayer.dll 0x1009C1F0 accepts a digit prefix followed by '.' or NUL.
			const char* text = name.c_str();
			if (*text)
			{
				const char* end = text;
				while (*end >= '0' && *end <= '9') ++end;
				if (*end == 0 || *end == '.')
				{
					const int index = atoi(text);
					if (index >= m_elements.size()) return false;
					*val = get_element(index);
					return true;
				}
			}
			return as_object::get_member(name, val);
		}

		virtual void	set_member(const tu_stringi& name, const as_value& val)
		{
			// SwiffPlayer.dll 0x1009C560 accepts only a nonempty, all-digit index.
			// Other names, including length and decimal suffixes, use the object bag.
			const char*	s = name.c_str();
			if (*s)
			{
				const char* end = s;
				while (*end >= '0' && *end <= '9') ++end;
				if (*end == 0)
				{
					set_element(atoi(s), val);
					return;
				}
			}
			as_object::set_member(name, val);
		}
	};

	//  see gameswf_action.h.
	as_object*	create_array_object()
	{
		return new as_array_object;
	}

	void	array_object_push(as_object* arr, const as_value& v)
	{
		as_array_object*	a = static_cast<as_array_object*>(arr);
		a->m_elements.push_back(v);
	}

	const array<as_value>& array_object_values(as_object_interface* arr)
	{
		return static_cast<as_array_object*>(arr)->m_elements;
	}

	static as_array_object*	array_this(const fn_call& fn)
	{
		// [CHOSEN, not recovered] S3 bridge: adjust the native interface to array storage.
		return static_cast<as_array_object*>(fn.this_ptr->cast_to_as_object());
	}

	void	array_push(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C8B0.
		as_array_object*	a = array_this(fn);
		for (int i = 0; i < fn.nargs; i++)
		{
			a->m_elements.push_back(fn.arg(i));
		}
		fn.result->set_int(a->m_elements.size());
	}

	void	array_pop(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C880: empty arrays leave the result untouched.
		as_array_object*	a = array_this(fn);
		if (a->m_elements.size() == 0) return;
		*fn.result = a->m_elements.back();
		a->m_elements.pop_back();
	}

	void	array_shift(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C940: empty arrays leave the result untouched.
		as_array_object*	a = array_this(fn);
		if (a->m_elements.size() == 0) return;
		*fn.result = a->get_element(0);
		a->m_elements.remove(0);
	}

	void	array_unshift(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009CAB0 inserts successive arguments at successive indices.
		as_array_object*	a = array_this(fn);
		const int	n = fn.nargs;
		for (int i = 0; i < n; i++)
		{
			a->m_elements.insert(i, fn.arg(i));
		}
		fn.result->set_int(a->m_elements.size());
	}

	// [CHOSEN, not recovered] S3 bridge to the current String factory.
	static void array_string_result(as_value* result, const tu_string& text)
	{
		as_object_interface* string = create_dice_string_object(NULL, 0);
		assign_dice_string_object_narrow(string, text.c_str());
		result->set_as_object_interface(string);
	}

	static void	array_join_impl(as_array_object* a, const char* sep, as_value* result, bool trailing_separator)
	{
		tu_string	s;
		for (int i = 0; i < a->m_elements.size(); i++)
		{
			if (!trailing_separator && i > 0) s += sep;
			s += a->get_element(i).to_tu_string();
			if (trailing_separator) s += sep;
		}
		array_string_result(result, s);
	}

	void	array_join(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C2C0 appends a separator after every element.
		as_array_object*	a = array_this(fn);
		tu_string	sep = ",";
		if (fn.nargs > 0) sep = fn.arg(0).to_tu_string();
		array_join_impl(a, sep.c_str(), fn.result, true);
	}

	void	array_to_string(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C4C0 inserts commas only between elements.
		as_array_object*	a = array_this(fn);
		array_join_impl(a, ",", fn.result, false);
	}

	void	array_reverse(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C5F0 mutates in place without assigning a result.
		as_array_object*	a = array_this(fn);
		for (int i = 0, j = a->m_elements.size() - 1; i < j; i++, j--)
		{
			as_value	t = a->get_element(i);
			a->set_element(i, a->get_element(j));
			a->set_element(j, t);
		}
	}

	void	array_slice(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009D0A0: even the default end excludes the last element.
		as_array_object*	a = array_this(fn);
		int	start = 0;
		int	end = a->m_elements.size() - 1;
		if (fn.nargs != 0)
		{
			start = bf2x87::toIntTruncate(fn.arg(0).to_number());
			if (fn.nargs > 1)
			{
				end = bf2x87::toIntTruncate(fn.arg(1).to_number());
				if (end < 0) end += a->m_elements.size();
				if (end >= a->m_elements.size()) end = a->m_elements.size() - 1;
			}
		}
		smart_ptr<as_array_object>	r = new as_array_object;
		for (int i = start; i < end; i++)
		{
			r->m_elements.push_back(a->get_element(i));
		}
		fn.result->set_as_object_interface(r.get_ptr());
	}

	void	array_splice(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C960 mutates the array and leaves the result untouched.
		if (fn.nargs < 2)
		{
			log_error("Array::splice needs atleast two arguments\n");
			return;
		}
		as_array_object*	a = array_this(fn);
		int	start = bf2x87::toIntTruncate(fn.arg(0).to_number());
		if (start >= a->m_elements.size()) start = a->m_elements.size() - 1;
		int	count = bf2x87::toIntTruncate(fn.arg(1).to_number());
		// LEA at 0x1009C9E2 adds in a 32-bit register before the signed comparison.
		const Uint32 end_bits = static_cast<Uint32>(start) + static_cast<Uint32>(count);
		int end;
		memcpy(&end, &end_bits, sizeof(end));
		if (end > a->m_elements.size())
			count = bf2x87::lowWord(static_cast<std::int64_t>(a->m_elements.size()) - start);
		for (int i = 0; i < count; ++i) a->m_elements.remove(start);
		for (int i = 2; i < fn.nargs; ++i) a->m_elements.insert(start++, fn.arg(i));
	}

	void	array_concat(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C770 expects an Array receiver and Array arguments.
		as_array_object*	a = array_this(fn);
		IF_VERBOSE_ACTION(log_msg("-- Array join \n"));
		smart_ptr<as_array_object>	r = new as_array_object;
		for (int k = 0; k < fn.nargs + 1; ++k)
		{
			as_array_object* source = a;
			if (k != 0) source = static_cast<as_array_object*>(
				fn.arg(k - 1).get_type() == as_value::OBJECT ? fn.arg(k - 1).m_object_value : NULL);
			for (int i = 0; i < source->m_elements.size(); ++i)
				r->m_elements.push_back(source->get_element(i));
		}
		fn.result->set_as_object_interface(r.get_ptr());
	}

	static int array_compare(const void* left, const void* right)
	{
		// SwiffPlayer.dll 0x1009C1A0 converts the right operand first.
		const char* rhs = static_cast<const as_value*>(right)->to_string();
		const char* lhs = static_cast<const as_value*>(left)->to_string();
		// [CHOSEN, not recovered] S3 bridge: native CRT case mapping/locale remains in use.
		return _stricmp(lhs, rhs);
	}

	void	array_sort(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009C1D0 ignores arguments and leaves the result untouched.
		as_array_object*	a = array_this(fn);
		qsort(a->m_elements.size() ? &a->m_elements[0] : NULL,
			a->m_elements.size(), sizeof(as_value), array_compare);
	}


	//
	// as_as_function
	//

	// [CHOSEN, not recovered] S3 bridge: functions are not native object-interface
	// subclasses. This adapter exposes retail's separate static bag and prototype.
	struct function_properties : public as_object
	{
		function_properties()
		{
			m_members.clear();
			m_instance_prototype = new as_object;
			m_instance_prototype->add_ref();
		}

		virtual bool get_member(const tu_stringi& name, as_value* val)
		{
			// SwiffPlayer.dll 0x1008E8E0: static members precede prototype lookup.
			if (strcmp(name.c_str(), "prototype") == 0)
			{
				val->set_as_object_interface(m_instance_prototype);
				return true;
			}
			stringi_hash<as_member>::const_iterator member = m_members.find(name);
			if (member != m_members.end())
			{
				if (val) *val = member.get_value().m_value;
				return true;
			}
			return m_instance_prototype && m_instance_prototype->get_member(name, val);
		}

		virtual void set_member(const tu_stringi& name, const as_value& val)
		{
			// SwiffPlayer.dll 0x10091CA0 accepts only OBJECT for a new prototype.
			if (strcmp(name.c_str(), "prototype") == 0)
			{
				as_object_interface* proto = val.get_type() == as_value::OBJECT ? val.m_object_value : NULL;
				// [CHOSEN, not recovered] S3 bridge: retain native ownership of this link.
				if (proto) proto->add_ref();
				if (m_instance_prototype) m_instance_prototype->drop_ref();
				m_instance_prototype = proto;
				return;
			}
			m_members.set(name, as_member(val));
		}
	};

	as_as_function::as_as_function(action_buffer* ab, as_environment* env, int start,
		const array<with_stack_entry>& with_stack)
		: m_action_buffer(ab), m_env(env), m_with_stack(with_stack), m_start_pc(start),
		m_length(0), m_is_function2(false), m_local_register_count(0), m_function2_flags(0),
		m_properties(new function_properties)
	{
		// SwiffPlayer.dll 0x1008E870 eagerly creates the prototype object.
		// [CHOSEN, not recovered] S3 bridge: m_properties includes the function adapter.
		m_properties->add_ref();
		assert(m_action_buffer);
	}

	void as_as_function::lazy_create_properties()
	{
		// [CHOSEN, not recovered] S3 bridge for existing native callers; construction
		// now creates the retail prototype eagerly. Never recreate a script-cleared prototype.
		assert(m_properties);
	}

	void as_as_function::set_function2_flags(uint16 flags)
	{
		// SwiffPlayer.dll 0x10094BDD-0x10094D80 builds this map in declaration order.
		// The native setter adapts the retained flags interface to that storage.
		assert(m_is_function2);
		m_function2_flags = flags;
		m_preload_registers.clear();
		int reg = 1;
		if (flags & 0x01) m_preload_registers.set(reg++, tu_string("this"));
		if (flags & 0x04) m_preload_registers.set(reg++, tu_string("arguments"));
		if (flags & 0x10) m_preload_registers.set(reg++, tu_string("super"));
		if (flags & 0x40) m_preload_registers.set(reg++, tu_string("_root"));
		if (flags & 0x80) m_preload_registers.set(reg++, tu_string("_parent"));
		if (flags & 0x100) m_preload_registers.set(reg++, tu_string("_global"));
	}

	void	as_as_function::operator()(const fn_call& fn)
	// Dispatch.
	{
		as_environment*	our_env = m_env;
		if (our_env == NULL)
		{
			our_env = fn.env;
		}
		assert(our_env);

		// SwiffPlayer.dll 0x10095790 / 0x10095840 binds the exact receiver. Super
		// belongs to call_method's saved environment slot, not to the function's first owner.
		as_object_interface* this_obj = fn.this_ptr;
		// SwiffPlayer.dll 0x10095840 does not add a local frame for function2.
		int	local_stack_top = our_env->get_local_frame_top();
		// The ordinary dispatch at 0x10095790 also appends arguments without a barrier.

		// [CHOSEN, not recovered] S3 bridge: explicitly captured native functions still borrow
		// the calling register bank. Declarations at 0x10094AEA/0x100952C9 capture no environment.
		// [RETIRE-WHEN: swf-s3-call-environment] Replace the native capture/register-bank
		// and per-tree-root adapters with the recovered owners, then validate nested
		// menu calls, LAN CREATE callbacks and registers through real movie execution.
		as_environment*	call_bank = (fn.env ? fn.env : our_env)->register_bank();
		struct register_bank_scope
		{
			as_environment*	m_e;
			as_environment*	m_saved;
			~register_bank_scope() { m_e->m_register_env = m_saved; }
		};
		register_bank_scope	bank_scope = { our_env, our_env->m_register_env };
		our_env->m_register_env = (call_bank == our_env) ? NULL : call_bank;

		//  A DefineFunction2 call borrows the environment's register bank
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
			//  no fresh registers - the environment's bank, saved above.

			// Handle the explicit args. the shipped loop writes each declared argument
			// the caller passed into ITS register, register 0 included - it never makes a named local
			// of a function2 argument. No stock movie declares one without a register.
			int	args_to_pass = imin(fn.nargs, m_args.size());
			for (int i = 0; i < args_to_pass; i++)
			{
				*(our_env->register_ptr(m_args[i].m_register)) = fn.arg(i);
			}

			// SwiffPlayer.dll 0x100958FB-0x10096136 visits the register/name hash.
			for (hash<int, tu_string>::const_iterator it = m_preload_registers.begin();
				it != m_preload_registers.end(); ++it)
			{
				const int reg = it.get_key();
				const tu_string name = it.get_value();
				if (name == "this")
					*our_env->register_ptr(reg) = as_value(this_obj);
				else if (name == "arguments")
					*our_env->register_ptr(reg) = as_value("arguments unimplemented");
				else if (name == "super")
					*our_env->register_ptr(reg) = as_value(our_env->m_super_ctor);
				else if (name == "_root")
				{
					// SwiffPlayer.dll 0x10095D59 publishes even a null OBJECT.
					*our_env->register_ptr(reg) = as_value(get_global_root_movie());
				}
				else if (name == "_parent")
					*our_env->register_ptr(reg) = as_value("_parent unimplemented");
				else if (name == "_global")
				{
					as_value global;
					s_modules.get("_global", &global);
					*our_env->register_ptr(reg) = global;
				}
			}
		}

		// Execute the actions.
		as_object_interface* saved_this = our_env->m_this_object;
		our_env->m_this_object = this_obj;
		m_action_buffer->execute(our_env, m_start_pc, m_length, fn.result, m_with_stack, m_is_function2);

		// Clean up stack frame.
		if (!m_is_function2) our_env->set_local_frame_top(local_stack_top);
		our_env->m_this_object = saved_this;

		if (m_is_function2)
		{
			//  r0..r9 back as they were before the call - the shipped
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
		// SwiffPlayer.dll 0x1008E990 installs the lookup's context for this call,
		// restoring the environment slot before copying the result to the caller.
		// [CHOSEN, not recovered] S3 bridge: native Key listeners still allow a null environment.
		as_as_function* saved_super = env ? env->m_super_ctor : NULL;
		if (env) env->m_super_ctor = s_call_super;
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

		if (env) env->m_super_ctor = saved_super;
		return val;
	}


	as_value	call_method0(
		const as_value& method,
		as_environment* env,
		as_object_interface* this_ptr)
	{
		// SwiffPlayer.dll 0x1008EAA0: zero arguments, first index is the operand count.
		return call_method(method, env, this_ptr, 0, env->get_top_index() + 1);
	}
		
	const char*	call_method_expression(as_environment* env, as_object_interface* receiver,
		const char* expression)
	{
		// SwiffPlayer.dll 0x10092220: tokenization precedes lookup and argument pushes.
		char text[1000];
		strncpy(text, expression, sizeof(text));
		text[sizeof(text) - 1] = 0;
		array<const char*> tokens;
		char quote = 0;
		bool in_token = false;
		for (char* cursor = text; *cursor; ++cursor)
		{
			const char c = *cursor;
			if (c == quote)
			{
				*cursor = 0;
				quote = 0;
				in_token = false;
			}
			else if (in_token)
			{
				if (quote == 0 && (c == ')' || c == '(' || c == ',' || c == ' '))
				{
					*cursor = 0;
					in_token = false;
				}
			}
			else if (c == '\'' || c == '"')
			{
				quote = c;
				*cursor = '\'';
				tokens.push_back(cursor);
				in_token = true;
			}
			else if (c == ' ' || c == ',')
			{
				*cursor = 0;
			}
			else
			{
				tokens.push_back(cursor);
				in_token = true;
			}
		}

		array<with_stack_entry> with_stack;
		as_value method = env->get_variable(tu_string(tokens[0]), with_stack);
		const int nargs = tokens.size() - 1;
		for (int i = nargs; i > 0; --i)
		{
			const char* argument = tokens[i];
			if (*argument == '\'') env->push(argument + 1);
			else env->push(atof(argument));
		}
		as_value result = call_method(method, env, receiver, nargs, env->get_top_index());
		env->drop(nargs);
		static tu_string result_text;
		result_text = result.to_tu_string();
		return result_text.c_str();
	}

	const char*	call_method_expression(as_environment* env, as_object_interface* receiver,
		const Uint16* expression)
	{
		// SwiffPlayer.dll 0x100924F0 converts UTF-16 before calling the narrow parser.
		tu_string narrow(expression);
		return call_method_expression(env, receiver, narrow.c_str());
	}


	//
	// sound object
	//
	// The three method bodies below exist in SwiffPlayer.dll (0x1008A1B0,
	// 0x1008A1F0, 0x1008C6E0) but nothing in the image references them: retail
	// registers no Sound class (action_init 0x10090FF0), so scripts cannot reach
	// them. They are kept as recovered bodies, equally unreferenced here.

	struct sound_as_object : public as_object
	{
		tu_string sound;
		int sound_id;
	};


	void	sound_start(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008A1B0: handler lookup/guard precedes receiver access.
		IF_VERBOSE_ACTION(log_msg("-- start sound \n"));
		sound_handler* s = get_sound_handler();
		if (s != NULL)
		{
			sound_as_object*	so = (sound_as_object*) (as_object*) fn.this_ptr;
			assert(so);
			s->play_sound(so->sound_id, 1);
		}
	}


	void	sound_stop(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008A1F0: arguments and result are untouched.
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
		// SwiffPlayer.dll 0x1008C6E0: refusal precedes name conversion/import lookup;
		// only a sound resource changes sound_id, and the result slot is untouched.
		IF_VERBOSE_ACTION(log_msg("-- attach sound \n"));
		if (fn.nargs < 1)
		{
			log_error("attach sound needs one argument\n");
			return;
		}

		sound_as_object*	so = (sound_as_object*) (as_object*) fn.this_ptr;
		assert(so);

		so->sound = fn.arg(0).to_tu_string();

		// [CHOSEN, not recovered] S3 bridge: resolve the process-global root read
		// at 0x1008C733 through this native movie tree instead.
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

	MATH_WRAP_FUNC1(fabs);  // SwiffPlayer.dll 0x1008C7D0.
	MATH_WRAP_FUNC1(acos);  // SwiffPlayer.dll 0x1008C830.
	MATH_WRAP_FUNC1(asin);  // SwiffPlayer.dll 0x1008C890.
	MATH_WRAP_FUNC1(atan);  // SwiffPlayer.dll 0x1008C8F0.
	MATH_WRAP_FUNC1(ceil);  // SwiffPlayer.dll 0x1008C950.
	MATH_WRAP_FUNC1(floor); // SwiffPlayer.dll 0x1008CA90.
	MATH_WRAP_FUNC1(log);   // SwiffPlayer.dll 0x1008CB00.
	MATH_WRAP_FUNC1(sqrt);  // SwiffPlayer.dll 0x1008CBC0.
	MATH_WRAP_FUNC1(tan);   // SwiffPlayer.dll 0x1008CC20.

	void math_cos(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008C9C0: FCOS leaves finite |x| >= 2^63 unchanged.
		const double arg = fn.arg(0).to_number();
		// [CHOSEN, not recovered] S3 bridge: retain the native math library for
		// in-range arguments; its argument reduction/rounding is not x87-exact.
		const double result = isfinite(arg) && fabs(arg) >= 9223372036854775808.0
			? arg : cos(arg);
		fn.result->set_double(result);
	}

	void math_exp(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008CA20: the x87 reduction subtracts infinity from
		// itself, so either infinity produces NaN rather than exp's limit value.
		const double arg = fn.arg(0).to_number();
		// [CHOSEN, not recovered] S3 bridge: finite results still use the native
		// math library rather than the retail F2XM1/FSCALE precision contract.
		const double result = isfinite(arg) ? exp(arg) : arg - arg;
		fn.result->set_double(result);
	}

	void math_sin(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008CB60: FSIN leaves finite |x| >= 2^63 unchanged.
		const double arg = fn.arg(0).to_number();
		// [CHOSEN, not recovered] S3 bridge: retain the native math library for
		// in-range arguments; its argument reduction/rounding is not x87-exact.
		const double result = isfinite(arg) && fabs(arg) >= 9223372036854775808.0
			? arg : sin(arg);
		fn.result->set_double(result);
	}

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
	MATH_WRAP_FUNC2_EXP(atan2, (atan2(arg0, arg1))); // SwiffPlayer.dll 0x1008CC80.
	MATH_WRAP_FUNC2_EXP(max, (arg0 > arg1 ? arg0 : arg1)); // SwiffPlayer.dll 0x1008CCF0.
	MATH_WRAP_FUNC2_EXP(min, (arg0 < arg1 ? arg0 : arg1)); // SwiffPlayer.dll 0x1008CD70.
	MATH_WRAP_FUNC2_EXP(pow, (pow(arg0, arg1))); // SwiffPlayer.dll 0x1008CDF0.

	// A couple of oddballs.
	void	math_random(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008AB80 multiplies the unsigned generator result.
		fn.result->set_double(double(tu_random::next_random()) * 2.3283064370807974e-10);
	}
	void	math_round(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008CE70.
		// round argument to nearest int.
		double	arg0 = fn.arg(0).to_number();
		fn.result->set_double(floor(arg0 + 0.5));
	}
	
	void math_init()
	{
		// SwiffPlayer.dll 0x1008EB30: constants and methods in registration order.
		// [CHOSEN, not recovered] Native object storage for the module value.
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

		// SwiffPlayer.dll 0x1008F7ED publishes into the module table.
		s_modules.set("math", as_value(math_obj));
	}
		
	
	//
	// key object
	//


	struct key_as_object : public as_object
	{
		// SwiffPlayer.dll 0x1008F900/0x1008FAB0 accepts codes 0 through 223.
		enum { RETAIL_KEY_COUNT = 224 };
		Uint8	m_keymap[key::KEYCOUNT / 8 + 1];	// bit-array
		array<weak_ptr<as_object_interface> >	m_listeners;
		int	m_last_key_pressed;
		// SwiffPlayer.dll 0x10092580 destroys listeners before members;
		// 0x10092550 is its deleting veneer. [CHOSEN, not recovered] S3 bridge:
		// implicit destruction uses the native weak listeners and as_object base.

		key_as_object()
			:
			m_last_key_pressed(0)
		{
			// SwiffPlayer.dll 0x1008F850: empty listeners, key bitmap and last key.
			// [CHOSEN, not recovered] S3 bridge: native weak-listener storage and
			// the legacy bitmap capacity; only codes 0..223 are notified by retail.
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
			// SwiffPlayer.dll 0x1008F900: update state before visiting the saved count.
			// [CHOSEN, not recovered] S3 bridge: native weak/strong listener ownership.
			if (code < 0 || code >= RETAIL_KEY_COUNT) return;

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
			// SwiffPlayer.dll 0x1008FAB0: release does not change the last pressed code.
			// [CHOSEN, not recovered] S3 bridge: native weak/strong listener ownership.
			if (code < 0 || code >= RETAIL_KEY_COUNT) return;

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
			// SwiffPlayer.dll 0x1008D700: remove expired entries backwards.
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
			// SwiffPlayer.dll 0x1008D760: clean, reject duplicates, then append weakly.
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
			// SwiffPlayer.dll 0x1008D840: clean, then erase every match backwards.
			cleanup_listeners();

			for (int i = m_listeners.size() - 1; i >= 0; i--)
			{
				if (m_listeners[i] == listener)
				{
					m_listeners.remove(i);
				}
			}
		}

		// SwiffPlayer.dll 0x1008A270.
		int	get_last_key_pressed() const { return m_last_key_pressed; }
	};


	void	key_add_listener(const fn_call& fn)
	// Add a listener (first arg is object reference) to our list.
	// Listeners will have "onKeyDown" and "onKeyUp" methods
	// called on them when a key changes state.
	{
		// SwiffPlayer.dll 0x1008D8B0 accepts OBJECT only, not script functions.
		if (fn.nargs < 1)
		{
			log_error("key_add_listener needs one argument (the listener object)\n");
			return;
		}

		as_object_interface*	listener = fn.arg(0).get_type() == as_value::OBJECT
			? fn.arg(0).m_object_value : NULL;
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
		// SwiffPlayer.dll 0x1008B2F0.
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
	// SwiffPlayer.dll 0x1008AC00.
	{
		key_as_object*	ko = (key_as_object*) (as_object*) fn.this_ptr;
		assert(ko);

		fn.result->set_int(ko->get_last_key_pressed());
	}

	static async_key_state_callback s_async_key_state_callback = NULL;

	void set_async_key_state_callback(async_key_state_callback callback)
	{
		s_async_key_state_callback = callback;
	}

	// SwiffPlayer.dll 0x1008A230. Only Enter queries (and consumes) OS state.
	bool is_key_down(int code)
	{
		if (code < 0 || code >= 224 || code != 13) return false;
		if (s_async_key_state_callback) return s_async_key_state_callback(code) == 0x8001;
#if defined(_WIN32)
		return static_cast<unsigned short>(GetAsyncKeyState(code)) == 0x8001;
#else
		// [CHOSEN, not recovered] Outside Windows the key state comes only from the host adapter,
		// which SwiffPlayer installs when it initializes (the macOS client is the one POSIX client
		// that builds SwiffPlayer). With no adapter installed (a bare test harness) Enter reads up.
		return false;
#endif
	}

	void	key_is_down(const fn_call& fn)
	// Publishes whether Enter is held with a press since the previous query.
	{
		if (fn.nargs < 1)
		{
			log_error("key_is_down needs one argument (the key code)\n");
			return;
		}

		// SwiffPlayer.dll 0x1008CEE0: FISTP truncation, then Enter edge query.
		const int code = bf2x87::toIntTruncate(fn.arg(0).to_number());
		fn.result->set_bool(is_key_down(code));
	}

	void	key_is_toggled(const fn_call& fn)
	// Given the keycode of NUM_LOCK or CAPSLOCK, returns true if
	// the associated state is on.
	{
		// SwiffPlayer.dll 0x1008AC10 always publishes false.
		fn.result->set_bool(false);
	}

	void	key_remove_listener(const fn_call& fn)
	// Remove a previously-added listener.
	{
		// SwiffPlayer.dll 0x1008D900 accepts OBJECT only, not script functions.
		if (fn.nargs < 1)
		{
			log_error("key_remove_listener needs one argument (the listener object)\n");
			return;
		}

		as_object_interface*	listener = fn.arg(0).get_type() == as_value::OBJECT
			? fn.arg(0).m_object_value : NULL;
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
		// SwiffPlayer.dll 0x1008FC50: constants and methods in registration order.
		// [CHOSEN, not recovered] S3 bridge: use the current native Key/global bags.
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
		KEY_CONST(_0);
		KEY_CONST(_1);
		KEY_CONST(_2);
		KEY_CONST(_3);
		KEY_CONST(_4);
		KEY_CONST(_5);
		KEY_CONST(_6);
		KEY_CONST(_7);
		KEY_CONST(_8);
		KEY_CONST(_9);

		// methods
		key_obj->set_member("addListener", &key_add_listener);
		key_obj->set_member("getAscii", &key_get_ascii);
		key_obj->set_member("getCode", &key_get_code);
		key_obj->set_member("isDown", &key_is_down);
		key_obj->set_member("isToggled", &key_is_toggled);
		key_obj->set_member("removeListener", &key_remove_listener);

		// SwiffPlayer.dll 0x10090CA1 publishes into the module table.
		s_modules.set("Key", as_value(key_obj));
	}

	void	add_key_listener(as_object_interface* listener)
	{
		// SwiffPlayer.dll 0x10090D00: hold the module value through the call; OBJECT only.
		static const tu_string name("Key");
		as_value module;
		s_modules.get(name, &module);
		if (module.get_type() == as_value::OBJECT)
		{
			// [CHOSEN, not recovered] S3 bridge: adjust the native object interface.
			static_cast<key_as_object*>(module.m_object_value->cast_to_as_object())->add_listener(listener);
		}
	}

	void	remove_key_listener(as_object_interface* listener)
	{
		// SwiffPlayer.dll 0x10090DF0: no initialization or fallback to _global.Key.
		static const tu_string name("Key");
		as_value module;
		s_modules.get(name, &module);
		if (module.get_type() == as_value::OBJECT)
		{
			// [CHOSEN, not recovered] S3 bridge: adjust the native object interface.
			static_cast<key_as_object*>(module.m_object_value->cast_to_as_object())->remove_listener(listener);
		}
	}

	int	get_last_key_pressed()
	{
		// SwiffPlayer.dll 0x10090EE0: absent/non-OBJECT module yields zero.
		static const tu_string name("Key");
		as_value module;
		s_modules.get(name, &module);
		if (module.get_type() == as_value::OBJECT)
		{
			// [CHOSEN, not recovered] S3 bridge: adjust the native object interface.
			return static_cast<key_as_object*>(module.m_object_value->cast_to_as_object())->get_last_key_pressed();
		}
		return 0;
	}


	void	notify_key_event(key::code k, bool down)
	// External interface for the host to report key events.
	{
		action_init();	// @@ put this in some global init somewhere else...

		static tu_string	key_obj_name("Key");

		as_value	kval;
		// SwiffPlayer.dll 0x10091D40 reads the module, independent of _global.Key.
		s_modules.get(key_obj_name, &kval);
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


	void	as_global_object_ctor(const fn_call& fn)
	// Constructor for ActionScript class Object.
	// SwiffPlayer.dll 0x1008B180; native storage is the existing as_object bridge.
	{
		fn.result->set_as_object_interface(new as_object);
	}

	void	as_global_array_ctor(const fn_call& fn)
	// Constructor for ActionScript class Array.
	{
		// SwiffPlayer.dll 0x1008B1C0: arguments are ignored, unlike array literal 0x42.
		// [CHOSEN, not recovered] S3 bridge: retain the native factory/interface convention.
		smart_ptr<as_array_object> ao(new as_array_object);
		fn.result->set_as_object_interface(ao.get_ptr());
	}


	// SwiffPlayer.dll 0x1008A280 is RET; 0x10090FF0 binds it as ASSetPropFlags.
	// It neither inspects arguments nor writes a result or property flags.
	void	as_global_assetpropflags(const fn_call&)
	{
	}


	// Color instances expose their stored RGB value and a target color transform.
	void	color_get_rgb(const fn_call& fn);
	void	color_set_rgb(const fn_call& fn);

	struct as_color : public as_object
	{
		// Compiler deleting wrapper: SwiffPlayer.dll 0x100A97B0 forwards
		// to the object and reference-counted bases before conditional deletion.
		// Native target ownership remains the separate S3 bridge below.
		~as_color() override = default;
		int				m_rgb;		// +24 in the shipped object
		// [CHOSEN, not recovered] S3 bridge: retain native strong target ownership.
		smart_ptr<as_object_interface>	m_target;	// +28 (a raw pointer in the shipped object)

		as_color()
		{
			// SwiffPlayer.dll 0x100A96A0: getRGB before setRGB; factory sets the fields.
			// [CHOSEN, not recovered] S3 bridge: native parent/interface storage.
			set_member("getRGB", as_value(color_get_rgb));
			set_member("setRGB", as_value(color_set_rgb));
		}
	};

	void	color_set_rgb(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100A95A0: exact arity; leave the result untouched.
		if (fn.nargs != 1)
		{
			log_error("Color:setRGB needs one argument\n");
			return;
		}
		as_color*	c = static_cast<as_color*>(fn.this_ptr);
		if (c->m_target == NULL)
		{
			return;
		}
		c->m_rgb = bf2x87::toIntTruncate(fn.arg(0).to_number());

		cxform	cx;
		// [CHOSEN, not recovered] S3 bridge: adjust the native target interface
		// instead of retail's fixed character-subobject subtraction. Reload after
		// numeric conversion; an invalid/cleared target still faults on the copy.
		as_object_interface* target = c->m_target.get_ptr();
		movie*		mv = target ? target->to_movie() : NULL;
		character*	ch = mv ? mv->cast_to_character() : NULL;
		const Uint32 rgb = static_cast<Uint32>(c->m_rgb);
		cx = ch->get_cxform();
		cx.m_[0][0] = 0.0f;	cx.m_[0][1] = (float) ((rgb >> 16) & 0xFF);
		cx.m_[1][0] = 0.0f;	cx.m_[1][1] = (float) ((rgb >> 8) & 0xFF);
		cx.m_[2][0] = 0.0f;	cx.m_[2][1] = (float) (rgb & 0xFF);
		// cx.m_[3][*] (alpha) stays as copied - the shipped body leaves it alone.
		// Retail character tables share the eight-float copy at 0x1007EDB0.
		ch->set_cxform(cx);
	}

	void	color_get_rgb(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100A9570: read first, then release the old result twice.
		const int rgb = static_cast<as_color*>(fn.this_ptr)->m_rgb;
		fn.result->set_int(rgb);
	}

	void	color_ctor(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1008B280: register methods, clear target/RGB, inspect arg0.
		as_color*	c = new as_color;
		c->m_target = NULL;
		c->m_rgb = 0;
		if (fn.nargs != 0)
		{
			c->m_target = fn.arg(0).get_type() == as_value::OBJECT
				? fn.arg(0).m_object_value : NULL;
		}
		fn.result->set_as_object_interface(c);
	}


	// Binds an exported symbol to the class applied when its sprite loads.
	// The Object module publishes this method; definitions retain native class links.
	void	object_register_class(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x100997B0 leaves the result untouched, including refusal.
		if (fn.nargs != 2)
		{
			log_error("Object::registerClass need two arguments\n");
			return;
		}
		// 0x100997DF..0x100997F7 borrows the converted text, captures the
		// function, then copies that text for the export lookup.
		const char* symbol_text = fn.arg(0).to_string();
		as_as_function*	cls = fn.arg(1).to_as_function();
		const tu_string symbol(symbol_text);

		// SwiffPlayer.dll 0x100997FC uses the published root, independently
		// of the calling environment's target.
		movie_definition*	def = get_global_root_movie()->get_movie_definition();
		// Every movie_definition in this tree is a movie_definition_sub (movie_def_impl,
		// sprite_definition), which is where the export table lives.
		smart_ptr<resource>	res = static_cast<movie_definition_sub*>(def)->get_exported_resource(symbol);
		character_def*	cdef = res.get_ptr() ? res->cast_to_character_def() : NULL;
		// [CHOSEN, not recovered] S3 bridge: native definitions own their class
		// references until release_library_classes; retail stores this pointer raw.
		if (cls) cls->add_ref();
		if (cdef->m_registered_class)
		{
			cdef->m_registered_class->drop_ref();
		}
		cdef->m_registered_class = cls;
	}

	// the shipped action_init (sub_10090FF0) registers "MovieClip" as a
	// no-op C function (nullsub_10). Its only job is to exist: `class X extends MovieClip` then
	// resolves a superclass that is not a script function, and the extends handler leaves the
	// subclass prototype's __proto__ empty - exactly what the shipped handler does with it.
	void	as_global_movieclip_ctor(const fn_call& fn)
	// SwiffPlayer.dll 0x1008A150 is RET, bound by 0x10090FF0.
	{
		(void) fn;
	}


	void	action_init()
	// Create/hook built-ins.
	{
		if (s_inited == false)
		{
			s_inited = true;

			// SwiffPlayer.dll 0x10091012-0x10091021: the module table's _global is the
			// empty common object 0x1000B320, without as_object's own members.
			// [CHOSEN, not recovered] S3 bridge: the native type is as_object with its
			// member bag emptied (its object live count is native bookkeeping).
			// 0x10091030/0x100910E4/0x10091198 also link each module into the
			// collector list (slot +0xC); owner decision 2026-11-09 keeps it inactive.
			assert(s_global == NULL);
			s_global = new as_object;
			s_global->m_members.clear();
			// SwiffPlayer.dll 0x10091072 publishes _global before registering modules.
			s_modules.set("_global", as_value(s_global.get_ptr()));
			// SwiffPlayer.dll 0x10091126 / 0x100911DA publishes String then Object,
			// before global functions, constructor factories, Math and Key.
			s_modules.set("String", as_value(create_dice_string_class()));
			// [CHOSEN, not recovered] S3 bridge: flatten the retail Object class
			// into the native bag; its common base interface remains shared with as_object.
			as_object* object_class = new as_object;
			object_class->m_members.clear();
			object_class->m_members.set("registerClass", as_member(as_value(object_register_class)));
			s_modules.set("Object", as_value(object_class));

			// SwiffPlayer.dll 0x10090FF0 publishes no Sound class: the image has no
			// "attachSound" name, no Sound constructor and no reference to the
			// three method bodies 0x1008A1B0 / 0x1008A1F0 / 0x1008C6E0.

			// ASSetPropFlags
			register_global_function("ASSetPropFlags", as_global_assetpropflags);
			// SwiffPlayer.dll 0x10091238 / 0x10091247 registers MovieClip next.
			register_global_function("MovieClip", as_global_movieclip_ctor);

			// SwiffPlayer.dll 0x1009124C-0x100913F5: the two XML factories
			// precede String and use the independently recovered Expat-backed objects.
			register_constructor("XML", xml_new);
			register_constructor("XMLNode", xmlnode_new);
			register_constructor("String", string_ctor);
			register_constructor("Array", as_global_array_ctor);
			register_constructor("Boolean", as_global_boolean_ctor);
			register_constructor("Number", as_global_number_ctor);
			register_constructor("Color", color_ctor);
			register_constructor("Object", as_global_object_ctor);

			math_init();
			key_init();

		}
	}


	// Accessors shadow ordinary data members. Script calls use the current VM
	// environment; native host access retains the explicit environment adapter.
	static as_environment	s_property_env;

	static as_environment* property_environment(as_object_interface* self)
	// Retail objects store their environment at +0x24 (as_object constructor
	// 0x1009A36B) and return it from vtable slot +0x2C (0x10077DC0).
	// [CHOSEN, not recovered] issue #7: native objects store no environment.
	{
		// SwiffPlayer.dll 0x10099F23/0x10099DF3 uses the environment published by execute.
		if (s_current_environment) return s_current_environment;
		// [CHOSEN, not recovered] S3 bridge for native host reads/writes outside execute.
		// Use the receiver's movie environment where the current interface exposes one.
		movie* target = self->to_movie();
		if (target) return target->get_action_environment();
		return &s_property_env;
	}

	static as_value call_property(const as_value& method, as_environment* env,
		as_object_interface* self, const as_value& argument)
	{
		// SwiffPlayer.dll 0x1008EAD0 passes one value for getters as well as setters.
		{
			const as_value value(argument);
			env->push_val(value);
		}
		as_value result = call_method(method, env, self, 1, env->get_top_index());
		return result;
	}

	void	as_object::add_property(const tu_stringi& name, const as_value& getter, const as_value& setter)
	{
		// SwiffPlayer.dll 0x1009A0F0 / 0x100999C0: append both borrowed
		// pointers, including nulls. Duplicate names occupy distinct entries.
		const property_accessors accessors = { getter.to_as_function(), setter.to_as_function() };
		m_accessors.add(name, accessors);
	}
	void as_object::gc_mark_members()
	{
		// SwiffPlayer.dll 0x1000B4B0 visits only the ordinary member bag.
		if (m_gc_generation == get_gc_mark_generation()) return;
		m_gc_generation = get_gc_mark_generation();
		for (stringi_hash<as_member>::iterator it = m_members.begin(); it != m_members.end(); ++it)
			gc_mark_value(it->second.m_value);
	}

	void as_object::gc_mark()
	{
		// SwiffPlayer.dll 0x10077DD0: base members, explicit prototype,
		// constructor, instance prototype, in that order. Accessors are not marked.
		if (m_gc_generation == get_gc_mark_generation()) return;
		gc_mark_members();
		if (m_prototype) m_prototype->gc_mark();
		if (m_constructor != NULL) m_constructor->gc_mark();
		if (m_instance_prototype) m_instance_prototype->gc_mark();
	}

	void as_object::gc_release()
	{
		// SwiffPlayer.dll 0x1000B6B0: visit OBJECT members, then clear the bag.
		// Prototype, constructor and accessor storage is left alone.
		if (m_gc_generation == get_gc_release_generation()) return;
		m_gc_generation = get_gc_release_generation();
		for (stringi_hash<as_member>::iterator it = m_members.begin(); it != m_members.end(); ++it)
			if (it->second.m_value.get_type() == as_value::OBJECT && it->second.m_value.to_object())
				it->second.m_value.to_object()->gc_release();
		m_members.clear();
	}

	void as_as_function::gc_mark()
	{
		// SwiffPlayer.dll 0x1008C620 has no function generation/cycle guard.
		// [CHOSEN, not recovered] S3 bridge: m_properties carries the separate
		// retail function prototype and static-member storage in the native graph.
		if (!m_properties) return;
		if (m_properties->m_instance_prototype) m_properties->m_instance_prototype->gc_mark();
		for (stringi_hash<as_member>::iterator it = m_properties->m_members.begin();
			it != m_properties->m_members.end(); ++it)
			gc_mark_value(it->second.m_value);
	}

	as_object::as_object() : m_instance_prototype(NULL), m_prototype(NULL)
	{
		// SwiffPlayer.dll 0x1009A190 / 0x1009A320 registers both own members.
		// [CHOSEN, not recovered] S3 bridge: native interface/refcount storage and
		// separate accessor maps replace the original base classes and pair hash.
		m_members.set("registerClass", as_member(as_value(object_register_class)));
		// SwiffPlayer.dll 0x1009A376 increments after the base's registerClass
		// write, before registering the instance's addProperty method.
		++s_object_live_count;
		m_members.set("addProperty", as_member(as_value(as_object_addproperty)));
	}

	as_object::as_object(as_object_interface* proto) : as_object()
	{
		// SwiffPlayer.dll 0x100936C8 / 0x100944FC installs the instance's prototype link.
		m_instance_prototype = proto;
		// [CHOSEN, not recovered] S3 bridge: native links retain their objects.
		if (m_instance_prototype) m_instance_prototype->add_ref();
	}

	as_object::~as_object()
	{
		// SwiffPlayer.dll 0x10077D88 and Array's inline copy at 0x10077F72
		// decrement before accessor, constructor and member destruction.
		--s_object_live_count;
		// [CHOSEN, not recovered] S3 bridge: release the native strong prototype links.
		// 0x10077D70 then destroys accessor keys, the retained constructor and
		// ordinary members. Their reverse declaration order supplies that sequence.
		if (m_instance_prototype) m_instance_prototype->drop_ref();
		if (m_prototype) m_prototype->drop_ref();
	}

	void as_object::clear()
	{
		// [CHOSEN, not recovered] issue #7: clear both native prototype links and
		// the constructor while release_script_objects holds its registry snapshot.
		m_members.clear();
		m_constructor = NULL;
		if (m_instance_prototype)
		{
			m_instance_prototype->drop_ref();
			m_instance_prototype = NULL;
		}
		if (m_prototype)
		{
			m_prototype->drop_ref();
			m_prototype = NULL;
		}
	}

	void as_object::set_member(const tu_stringi& name, const as_value& val)
	{
		// SwiffPlayer.dll 0x1009A2A0: accessors precede the two special writes.
		if (set_property(name, val)) return;
		if (strcmp(name.c_str(), "__proto__") == 0)
		{
			as_object_interface* proto = val.get_type() == as_value::OBJECT ? val.m_object_value : NULL;
			// [CHOSEN, not recovered] S3 bridge: retain native ownership of the raw retail link.
			if (proto) proto->add_ref();
			if (m_prototype) m_prototype->drop_ref();
			m_prototype = proto;
		}
		else if (strcmp(name.c_str(), "__constructor__") == 0)
		{
			m_constructor = val.to_as_function();
		}
		else
		{
			m_members.set(name, as_member(val));
		}
	}

	bool as_object::get_member(const tu_stringi& name, as_value* val)
	{
		// SwiffPlayer.dll 0x10099FE0: accessors, special links, own bag, then both links.
		if (get_property(name, val))
		{
			s_call_super = m_constructor.get_ptr();
			return true;
		}
		if (strcmp(name.c_str(), "prototype") == 0)
		{
			val->set_as_object_interface(m_instance_prototype);
			return true;
		}
		if (strcmp(name.c_str(), "__proto__") == 0)
		{
			val->set_as_object_interface(m_prototype);
			return true;
		}
		if (strcmp(name.c_str(), "__constructor__") == 0)
		{
			*val = as_value(m_constructor.get_ptr());
			return true;
		}
		stringi_hash<as_member>::const_iterator member = m_members.find(name);
		if (member != m_members.end())
		{
			*val = member.get_value().m_value;
			s_call_super = m_constructor.get_ptr();
			return true;
		}
		if (m_instance_prototype && m_instance_prototype->get_member(name, val)) return true;
		if (m_prototype && m_prototype->get_member(name, val)) return true;
		return false;
	}

	// [CHOSEN, not recovered] S3 bridge for sprite callers outside this file.
	// These lookup-only helpers use the same unbounded prototype walk as the accessors.
	bool	as_object::find_getter(const tu_stringi& name, as_value* getter, int depth)
	{
		(void) depth;
		property_accessors accessors;
		if (m_accessors.get(name, &accessors) && accessors.getter)
		{
			*getter = as_value(accessors.getter);
			return true;
		}
		if (m_prototype != NULL)
		{
			as_object*	proto = m_prototype->cast_to_as_object();
			if (proto)
			{
				return proto->find_getter(name, getter);
			}
		}
		return false;
	}

	bool	as_object::find_setter(const tu_stringi& name, as_value* setter, int depth)
	{
		(void) depth;
		property_accessors accessors;
		if (m_accessors.get(name, &accessors) && accessors.setter)
		{
			*setter = as_value(accessors.setter);
			return true;
		}
		if (m_prototype != NULL)
		{
			as_object*	proto = m_prototype->cast_to_as_object();
			if (proto)
			{
				return proto->find_setter(name, setter);
			}
		}
		return false;
	}

	as_value	as_object::invoke_getter(const as_value& getter, as_object_interface* self,
		const as_value& argument)
	{
		as_environment* env = property_environment(self);
		as_value result = call_property(getter, env, self, argument);
		env->drop(1);
		return result;
	}

	void	as_object::invoke_setter(const as_value& setter, as_object_interface* self, const as_value& val)
	{
		as_environment* env = property_environment(self);
		call_property(setter, env, self, val);
		env->drop(1);
	}

	bool	as_object::get_property(const tu_stringi& name, as_value* val, as_object_interface* receiver)
	{
		// SwiffPlayer.dll 0x10099EB0: only a non-null script getter handles the name.
		if (!receiver) receiver = this;
		property_accessors accessors;
		if (m_accessors.get(name, &accessors) && accessors.getter)
		{
			const as_value getter(accessors.getter);
			*val = invoke_getter(getter, receiver, *val);
			return true;
		}
		for (as_object_interface* link = m_prototype; link; )
		{
			// [CHOSEN, not recovered] S3 bridge: adjust the native object interface.
			as_object* proto = link->cast_to_as_object();
			// A movie clip on the chain ends the walk "not found". Retail reads the accessor table
			// and link at the script-object offsets of the clip's secondary interface, where a
			// movie keeps unrelated empty state; retail 1.50 finds no getter there and does not
			// fault (docs/tracks/game/swf-proto-retail-probe.md, v2 test 2 red, tests 1/5 likewise).
			// get_member still finds the clip's ordinary members afterwards (test 1 green).
			if (!proto) return false;
			if (proto->get_property(name, val, receiver)) return true;
			link = proto->m_prototype;
		}
		return false;
	}

	bool	as_object::set_property(const tu_stringi& name, const as_value& val, as_object_interface* receiver)
	{
		// SwiffPlayer.dll 0x10099D80: preserve receiver through the recursive/outer walks.
		if (!receiver) receiver = this;
		property_accessors accessors;
		if (m_accessors.get(name, &accessors) && accessors.setter)
		{
			const as_value setter(accessors.setter);
			invoke_setter(setter, receiver, val);
			return true;
		}
		for (as_object_interface* link = m_prototype; link; )
		{
			as_object* proto = link->cast_to_as_object();
			// A movie clip on the chain ends the walk "not found", as in get_property: no setter
			// runs (probe v2 test 3 red) and set_member stores an own member (test 6 green).
			if (!proto) return false;
			if (proto->set_property(name, val, receiver)) return true;
			link = proto->m_prototype;
		}
		return false;
	}

	// The builtin itself. `this_ptr` is an as_object_interface* that need not be an as_object (a
	// sprite_instance is not), hence the virtual cast_to_as_object rather than a static_cast.
	void	as_object_addproperty(const fn_call& fn)
	{
		// SwiffPlayer.dll 0x1009A0F0: exact arity, no result assignment on either arm.
		if (fn.nargs != 3)
		{
			log_error("Object::addProperty needs three arguments\n");
			return;
		}
		// [CHOSEN, not recovered] S3 bridge: adjust the native object interface.
		as_object*	obj = fn.this_ptr->cast_to_as_object();
		// [CHOSEN, not recovered] A movie-clip receiver accepts the call and keeps nothing.
		// Retail writes the pair into the accessor table at the clip's secondary-interface offset,
		// where no lookup ever finds it: no fault, the getter is never found (probe v2 test 5).
		if (obj == NULL) return;
		// 0x1009A126 / 0x1009A136 capture the borrowed functions before
		// converting the name. Do not create retaining as_value temporaries.
		const as_object::property_accessors accessors = {
			fn.arg(1).to_as_function(), fn.arg(2).to_as_function()
		};
		const tu_stringi name(fn.arg(0).to_string());
		obj->m_accessors.add(name, accessors);
	}

	// [CHOSEN, not recovered] S3 bridge: the native host graph needs initialization
	// before the first advance. Retail's lookup_global_object itself never initializes.
	as_object*	get_global_object()
	{
		action_init();
		as_object_interface* global = lookup_global_object();
		return global ? global->cast_to_as_object() : NULL;
	}


	void	action_clear()
	{
		// [CHOSEN, not recovered] S3 bridge to issue #7: a native movie reset
		// invalidates the borrowed lookup context just as unloading the DLL would.
		s_call_super = NULL;
		if (s_inited)
		{
			s_inited = false;

			// [CHOSEN, not recovered] issue #7: release native module references too.
			s_modules.clear();
			s_constructors.clear();
			s_global->clear();
			s_global = NULL;
		}
	}


	//
	// properties by number
	//

	// SwiffPlayer.dll 0x100D6520 / 0x100D6DE0: construct these 22 names
	// in order and destroy them in reverse. The quality placeholder is literal.
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
	// SwiffPlayer.dll 0x1008CF60.
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
		// SwiffPlayer.dll 0x1008A290: signed bounds check, then one virtual write.
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
		// SwiffPlayer.dll 0x10091420 constructs the embedded action_buffer.
		action_buffer	m_buf;
		// SwiffPlayer.dll 0x10091490 destroys the buffer before the execute_tag base.
		virtual ~do_action() = default;

		void	read(stream* in)
		{
			// SwiffPlayer.dll 0x1008D9D0.
			m_buf.read(in);
		}

		virtual void	execute(movie* m)
		{
			// SwiffPlayer.dll 0x10091450 queues the buffer on the movie.
			m->add_action_buffer(&m_buf);
		}

		// SwiffPlayer.dll 0x10091470: actions are not replayed when seeking.
		virtual void execute_state(movie*) {}

		virtual bool	is_action_tag() const
		// Tell the caller that we are an action tag.
		{
			// SwiffPlayer.dll 0x10091480.
			return true;
		}
	};

	void	do_action_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		// SwiffPlayer.dll 0x10091E50: read the new buffer, then publish the tag.
		if (s_verbose_loader) log_msg("tag %d: do_action_loader\n", tag_type);

		IF_VERBOSE_PARSE(log_msg("-- actions in frame %d\n", m->get_loading_frame()));

		assert(in);
		assert(tag_type == 12);
		assert(m);
		
		do_action*	da = new do_action;
		da->read(in);

		m->add_execute_tag(da);
	}

	void execute_init_action(execute_tag* tag, as_environment* env)
	{
		// SwiffPlayer.dll 0x100818A3 calls action_buffer::execute (0x100961E0)
		// directly. [CHOSEN, not recovered] S3 bridge: the native definition
		// stores an execute_tag wrapper instead of the retail init-action record.
		static_cast<do_action*>(tag)->m_buf.execute(env);
	}

	
	//
	// do_init_action
	//


	// SwiffPlayer.dll 0x1006FB10: resolve the ID before allocating the buffer;
	// append remains definition-wide, independently of the unused lookup result.
	void	do_init_action_loader(stream* in, int tag_type, movie_definition_sub* m)
	{
		assert(tag_type == 59);

		int	sprite_character_id = in->read_u16();
		m->get_character_def(sprite_character_id);

		if (s_verbose_loader) log_msg("tag %d: do_init_action_loader\n", tag_type);
		IF_VERBOSE_PARSE(log_msg("-- init actions in sprite %d\n", sprite_character_id));

		do_action*	da = new do_action;
		da->read(in);
		m->add_init_action(da);
	}


	//
	// action_buffer
	//

	// Disassemble one instruction to the log.
	static void	log_disasm(const unsigned char* instruction_data);

	// SwiffPlayer.dll 0x1008A070; [INFERRED] source-level helper name.
	static double floor_float(float value) { return floor(double(value)); }

	action_buffer::action_buffer()
		:
		m_decl_dict_processed_at(-1)
	{
		// SwiffPlayer.dll 0x1008D9E0: empty bytes/dictionary, unprocessed dictionary.
	}


	void	action_buffer::read(stream* in)
	// SwiffPlayer.dll 0x1008D010.
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

			IF_VERBOSE_PARSE(log_msg("%4d\t", pc); log_disasm(&m_buffer[instruction_start]); );

			if (action_id == 0)
			{
				// end of action buffer.
				break;
			}
		}
	}


	void	action_buffer::process_decl_dict(int start_pc, int stop_pc)
	// SwiffPlayer.dll 0x1008B350.
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
		// SwiffPlayer.dll 0x100961E0 adds no local-frame barrier.
		array<with_stack_entry>	empty_with_stack;
		execute(env, 0, m_buffer.size(), NULL, empty_with_stack, false /* not function2 */);
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
	//  is_function2 no longer selects a register set: every register
	// operand addresses the environment's one bank, as in the shipped execute (0x100925A0).
	{
		(void)is_function2;
		action_init();	// @@ stick this somewhere else; need some global static init function
		// 0x100925B4/0x1009553B/0x10095562: save the caller's publication.
		as_environment* previous_environment = s_current_environment;
		s_current_environment = env;

		assert(env);
		// SwiffPlayer.dll 0x100925CD/0x10095541: preserve the entry operand count.
		const int original_stack_size = env->m_stack.size();

		// SwiffPlayer.dll 0x100925D6: each execution starts with an empty with stack.
		// The retained native signature still accepts the 2006 capture argument.
		(void)initial_with_stack;
		array<with_stack_entry>	with_stack;

		// SwiffPlayer.dll 0x100925A0 does not query the movie version at entry.

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
					// 0x1009555F: END restores operands but does not restore the target.
					s_current_environment = previous_environment;
					if (env->m_stack.size() != original_stack_size)
					{
						env->m_stack.resize(original_stack_size);
					}
					return;

				case 0x04:	// next frame.
				{
					// SwiffPlayer.dll 0x10092670: capture the selected receiver
					// across its frame query and dispatch the immediate numeric entry.
					movie* target = env->get_target();
					target->apply_goto_frame(target->get_current_frame() + 1);
					break;
				}

				case 0x05:	// prev frame.
				{
					// SwiffPlayer.dll 0x10092691 uses the same immediate entry.
					movie* target = env->get_target();
					target->apply_goto_frame(target->get_current_frame() - 1);
					break;
				}

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
					// SwiffPlayer.dll 0x100927B2 caches the destination before comparison.
					as_value& left = env->top(1);
					left.set_bool(left == env->top(0));
					env->drop(1);
					break;
				}
				case 0x0F:	// less than
				{
					// SwiffPlayer.dll 0x100927EF caches the destination before comparison.
					as_value& left = env->top(1);
					left.set_bool(left < env->top(0));
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
					// SwiffPlayer.dll 0x1009285B: shipped OR repeats the AND operation.
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
					// SwiffPlayer.dll 0x100928FB/0x10092903 converts right before left.
					as_value& destination = env->top(1);
					const tu_string& right = env->top(0).to_tu_string();
					const tu_string& left = destination.to_tu_string();
					destination.set_bool(left == right);
					env->drop(1);
					break;
				}
				case 0x14:	// string length
				{
					// SwiffPlayer.dll 0x1009292E counts narrow bytes, not UTF-8 characters.
					const int length = env->top(0).to_tu_string().length();
					env->top(0).set_int(length);
					break;
				}
				case 0x15:	// substring
				{
					// SwiffPlayer.dll 0x1009296C computes a temporary substring, then
					// discards it: only the length and starting-index operands are dropped.
					int	size = bf2x87::toIntTruncate(env->top(0).to_number());
					const Uint32 base_bits = static_cast<Uint32>(bf2x87::toIntTruncate(env->top(1).to_number())) - 1u;
					int base;
					memcpy(&base, &base_bits, sizeof(base));
					const tu_string&	str = env->top(2).to_tu_string();

					// Keep base within range.
					base = iclamp(base, 0, str.length());

					// Truncate if necessary.
					size = imin(str.length() - base, size);

					// @@ This can be done without new allocations if we get dirtier w/ internals
					// of as_value and tu_string...
					tu_string	new_string = str.c_str() + base;
					new_string.resize(size);

					env->drop(2);

					break;
				}
				case 0x17:	// pop
				{
					env->drop(1);
					break;
				}
				case 0x18:	// int
				{
					// SwiffPlayer.dll 0x10092A7D: floor followed by signed dword FISTP.
					as_value& value = env->top(0);
					value.set_int(bf2x87::toIntTruncate(floor(value.to_number())));
					break;
				}
				case 0x1C:	// get variable
				{
					// SwiffPlayer.dll 0x10092AD4-0x10092AFA: the lookup result
					// goes directly to the by-value push, with no retained result local.
					as_value var_name = env->pop();
					env->push_val(env->get_variable(var_name.to_tu_string(), with_stack));
					IF_VERBOSE_ACTION(
						// 0x10092B14/0x10092B2A converts the result before the name.
						const char* value = env->top(0).to_tu_string().c_str();
						const char* name = var_name.to_tu_string().c_str();
						log_msg("-- get var: %s=%s \n", name, value));

					break;
				}
				case 0x1D:	// set variable
				{
					// SwiffPlayer.dll 0x10092B5F-0x10092B99: no verbose conversion.
					const as_value& value = env->top(0);
					const tu_string& name = env->top(1).to_tu_string();
					env->set_variable(name, value, with_stack);

					env->drop(2);
					break;
				}
				case 0x20:	// set target expression
				{
					// SwiffPlayer.dll 0x10092B9C leaves both target and stack unchanged.
					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x21:	// string concat
				{
					// SwiffPlayer.dll 0x10092BB6 has no version-dependent conversion.
					as_value& left = env->top(1);
					left.string_concat(env->top(0).to_tu_string());
					env->drop(1);
					break;
				}
				case 0x22:	// get property
				{

					as_object_interface*	target = env->find_target(env->top(1));
					if (target)
					{
						// SwiffPlayer.dll 0x10092C38 uses a signed dword FISTP.
						// 0x10092C0C caches the destination before conversion and dispatch.
						as_value& result = env->top(1);
						const int property = bf2x87::toIntTruncate(env->top(0).to_number());
						result = get_property(target, property);
					}
					else
					{
						// SwiffPlayer.dll 0x10092C6C: failed target lookup yields NUMBER 0.
						env->top(1).set_double(0.0);
					}
					env->drop(1);
					break;
				}

				case 0x23:	// set property
				{

					as_object_interface*	target = env->find_target(env->top(2));
					if (target)
					{
						// SwiffPlayer.dll 0x10092D0D uses the same dword conversion.
						const as_value& value = env->top(0);
						const int property = bf2x87::toIntTruncate(env->top(1).to_number());
						set_property(target, property, value);
					}
					env->drop(3);
					break;
				}

				case 0x24:	// duplicate clip (sprite?)
				{
					// SwiffPlayer.dll 0x10092D6C-0x10092DB5: capture target,
					// convert depth, new name, then old name, before the clone call.
					movie* target = env->get_target();
					const Uint16 depth = static_cast<Uint16>(bf2x87::toInt64Truncate(env->top(0).to_number()));
					const tu_string& newname = env->top(1).to_tu_string();
					const tu_string& name = env->top(2).to_tu_string();
					target->clone_display_object(name, newname, depth);
					env->drop(3);
					break;
				}

				case 0x25:	// remove clip
				{
					// SwiffPlayer.dll 0x10092DD7 captures the target before conversion.
					movie* target = env->get_target();
					const tu_string& name = env->top(0).to_tu_string();
					target->remove_display_object(name);
					env->drop(1);
					break;
				}

				case 0x26:	// trace
				{
					// SwiffPlayer.dll 0x10092E0B converts directly; it never calls toString.
					log_msg("%s\n", env->top(0).to_string());
					env->drop(1);
					break;
				}

				case 0x27:	// start drag movie
				{
					movie::drag_state	st;

					// SwiffPlayer.dll 0x10092E5B-0x10092E91: a missing target logs the
					// error and continues with a null character.
					// [CHOSEN, not recovered] issue #7 native type adjustment: retail
					// subtracts 8 from any found interface without a type check
					// (0x10092E68), so a non-movie OBJECT would be used as a character;
					// the native cast treats it as an invalid target instead.
					as_object_interface* drag_target = env->find_target(env->top(0));
					st.m_character = drag_target ? drag_target->to_movie() : NULL;
					if (st.m_character == NULL)
					{
						log_error("error: start_drag of invalid target '%s'.\n",
							  env->top(0).to_string());
					}

					st.m_lock_center = env->top(1).to_bool();
					// SwiffPlayer.dll 0x10092EBF..0x10092F40 captures the mouse
					// offset before converting the bounds or removing any operands.
					if (!st.m_lock_center && st.m_character != NULL)
					{
						int x, y, buttons;
						get_global_root_movie()->get_mouse_state(&x, &y, &buttons);
						matrix world = st.m_character->get_world_matrix();
						point local;
						const float world_x = bf2x87::store(bf2x87::mul(bf2x87::Real(x), bf2x87::Real(20.0f)));
						const float world_y = bf2x87::store(bf2x87::mul(bf2x87::Real(y), bf2x87::Real(20.0f)));
						world.transform_by_inverse(&local, point(world_x, world_y));
						st.m_offset_x = local.m_x;
						st.m_offset_y = local.m_y;
					}
					st.m_bound = env->top(2).to_bool();
					if (st.m_bound)
					{
						// 0x10092F75..0x10093058 fetches the parent matrix once,
						// converts Y before X, and transforms each pair immediately.
						matrix parent_world = st.m_character->cast_to_character()->get_parent()->get_world_matrix();
						float y0 = bf2x87::store(bf2x87::mul(bf2x87::Real(env->top(5).to_number()), bf2x87::Real(20.0f)));
						float x0 = bf2x87::store(bf2x87::mul(bf2x87::Real(env->top(6).to_number()), bf2x87::Real(20.0f)));
						point bound;
						parent_world.transform(&bound, point(x0, y0));
						st.m_bound_x0 = bound.m_x;
						st.m_bound_y0 = bound.m_y;
						float y1 = bf2x87::store(bf2x87::mul(bf2x87::Real(env->top(3).to_number()), bf2x87::Real(20.0f)));
						float x1 = bf2x87::store(bf2x87::mul(bf2x87::Real(env->top(4).to_number()), bf2x87::Real(20.0f)));
						parent_world.transform(&bound, point(x1, y1));
						st.m_bound_x1 = bound.m_x;
						st.m_bound_y1 = bound.m_y;
						env->drop(4);
					}
					env->drop(3);

					// SwiffPlayer.dll 0x10093071 re-reads the global after conversions.
					movie*	root_movie = get_global_root_movie();

					if (root_movie && st.m_character)
					{
						root_movie->set_drag_state(st);
					}
					
					break;
				}

				case 0x28:	// stop drag movie
				{
					// SwiffPlayer.dll 0x100930B4: unchecked published-root dispatch.
					get_global_root_movie()->stop_drag();

					break;
				}

				case 0x29:	// string less than
				{
					// SwiffPlayer.dll 0x100930E1/0x100930E9: right before left.
					as_value& destination = env->top(1);
					const tu_string& right = env->top(0).to_tu_string();
					const tu_string& left = destination.to_tu_string();
					destination.set_bool(left < right);
					break;
				}

				case 0x30:	// random
				{
					// SwiffPlayer.dll 0x10093127/0x1009314D: signed FISTP, then unsigned DIV.
					int	max = bf2x87::toIntTruncate(env->top(0).to_number());
					if (max < 1) max = 1;
					env->top(0).set_double(double(tu_random::next_random() % static_cast<Uint32>(max)));
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
					// SwiffPlayer.dll 0x100931A2 sign-extends the first narrow byte.
					as_value& value = env->top(0);
					value.set_int(static_cast<signed char>(value.to_string()[0]));
					break;
				}
				case 0x33:	// chr
				{
					char	buf[2];
					// SwiffPlayer.dll 0x100931C7: _ftol's 64-bit result, then AL.
					const unsigned char byte = static_cast<unsigned char>(
						bf2x87::toInt64Truncate(env->top(0).to_number()));
					memcpy(&buf[0], &byte, sizeof(byte));
					buf[1] = 0;
					env->top(0).set_string(buf);
					break;
				}

				case 0x34:	// get timer
					// SwiffPlayer.dll 0x100931F1..0x10093214 stores the product
					// as float before the double floor wrapper at 0x1008A070.
					env->push(float(floor_float(bf2x87::store(bf2x87::mul(
						bf2x87::Real(env->m_target->get_timer()), bf2x87::Real(1000.0f))))));
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
					// SwiffPlayer.dll 0x1009326A-0x100932A5: perform the raw
					// lookup, report, then release the result without changing the stack.
					as_value obj_ptr = env->get_variable_raw(env->top(0).to_tu_string(), with_stack);
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
					// a function resolved through a `with` scope runs with
					// THAT object as `this`. mainMenu.swf's ScrollBar.createChildren does
					//   with (this.m_mcScrollVertical) { attachMovie("arrowUp", "arrowUp", getNextHighestDepth()); ... }
					// Stock 2006 passed this_ptr = NULL, so the sprite builtins fell back to the
					// environment's target - the root, when the method chain was started from a root-level
					// init action - and the arrows were attached to _root at depth 259+ with an identity
					// transform (retail run L). The shipped player attaches them to the with-object.
					// SwiffPlayer.dll 0x10093308 / 0x1009153B: lookup returns
					// the first with-scope value and its receiver, callable or not.
					as_object_interface*	with_this = env->m_this_object;
					if (env->top(0).get_type() == as_value::STRING)
					{
						// Function is a string; lookup the function.
						const tu_string&	function_name = env->top(0).to_tu_string();
						function = env->get_variable(function_name, with_stack, &with_this);

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
					int	nargs = bf2x87::toIntTruncate(env->top(1).to_number());
					if (with_this == NULL) with_this = env->m_target;
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
					int	nargs = bf2x87::toIntTruncate(env->pop().to_number());
					// SwiffPlayer.dll 0x100935D1-0x10093674: look up the private
					// factory table before resolving a script constructor.
					as_c_function_ptr factory = NULL;
					as_value new_obj;
					if (s_constructors.get(classname.to_tu_stringi(), &factory))
					{
						as_value factory_result;
						factory(fn_call(&factory_result, NULL, env, nargs, env->get_top_index()));
						new_obj.set_as_object_interface(factory_result.get_type() == as_value::OBJECT
							? factory_result.m_object_value : NULL);
					}
					else
					{
						as_value constructor = env->get_variable(classname.to_tu_string(), with_stack);
						if (as_as_function* ctor_as_func = constructor.to_as_function())
						{
							// SwiffPlayer.dll 0x100936C8/0x100936DE: install the
							// instance prototype, then call directly without a result slot.
							as_object_interface* proto = ctor_as_func->m_properties->m_instance_prototype;
							smart_ptr<as_object> instance(new as_object(proto));
							new_obj.set_as_object_interface(instance.get_ptr());
							(*ctor_as_func)(fn_call(NULL, instance.get_ptr(), env, nargs, env->get_top_index()));
						}
						else
						{
							// SwiffPlayer.dll 0x100936E3/0x1009373E: even a
							// failed construction pushes an OBJECT with a null pointer.
							log_error("can't create unregistered object '%s'\n", classname.to_string());
							new_obj.set_as_object_interface(NULL);
						}
					}

					// SwiffPlayer.dll 0x10093714-0x10093730: a result other than the
					// published root's object is linked into the collector list (common
					// slot +0xC, 0x10008F10) before the drop/push.
					// [CHOSEN, not recovered] owner decision 2026-11-09 (issue #7): the
					// collector list stays inactive; native reference counting owns it.
					env->drop(nargs);
					env->push(new_obj);
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
					// SwiffPlayer.dll 0x1009378A: pop count, then elements in index order.
					smart_ptr<as_array_object> obj(new as_array_object);
					int	array_size = bf2x87::toIntTruncate(env->pop().to_number());
					for (int i = 0; i < array_size; ++i)
					{
						char name[20];
						_snprintf(name, sizeof(name), "%d", i);
						// SwiffPlayer.dll 0x10093823-0x10093865 constructs the
						// name before popping, then releases the value before the name.
						const tu_stringi member_name(name);
						const as_value value = env->pop();
						obj->set_member(member_name, value);
					}
					env->push(as_value(static_cast<as_object_interface*>(obj.get_ptr())));
					break;
				}
				case 0x43:	// declare object  (ActionInitObject)
				{
					// SwiffPlayer.dll 0x100938AE: object literal, count then value/name pairs.
					smart_ptr<as_object>	obj(new as_object);
					int	num_props = bf2x87::toIntTruncate(env->pop().to_number());
					for (int i = 0; i < num_props; i++)
					{
						const as_value		value = env->pop();
						// SwiffPlayer.dll 0x1009391F/0x1009392D: value before name.
						// 0x1009392D-0x10093990 keeps the popped name alive
						// through the member write and name-string destruction.
						const as_value name_value = env->pop();
						const tu_stringi	name = name_value.to_tu_string().to_tu_stringi();
						obj->set_member(name, value);
					}
					as_value	result;
					result.set_as_object_interface(obj.get_ptr());
					env->push(result);
					break;
				}
				// SwiffPlayer.dll 0x100939AF / 0x100939C7 / 0x100939DF.
				case 0x44:	// type of
				case 0x45:	// get target
				case 0x46:	// enumerate
				{
					log_error("todo opcode: %02X\n", action_id);
					break;
				}
				case 0x47:	// add_t (typed)
				{
					// SwiffPlayer.dll 0x100939F7: only two numbers add numerically.
					// [CHOSEN, not recovered] S3 bridge: retain the 2006 NULLTYPE
					// alongside UNDEFINED as its native representation of retail tag zero.
					const as_value::type left = env->top(1).get_type();
					const as_value::type right = env->top(0).get_type();
					if (left == as_value::NUMBER && right == as_value::NUMBER)
					{
						env->top(1) += env->top(0);
					}
					else if (left == as_value::NUMBER && (right == as_value::UNDEFINED || right == as_value::NULLTYPE))
					{
						env->top(1) = env->top(1);
					}
					else if ((left == as_value::UNDEFINED || left == as_value::NULLTYPE) && right == as_value::NUMBER)
					{
						env->top(1) = env->top(0);
					}
					else
					{
						// SwiffPlayer.dll 0x10093A94-0x10093AAB: capture the
						// destination before the right operand's conversion callback.
						as_value& destination = env->top(1);
						destination.string_concat(env->top(0).to_tu_string());
					}
					env->drop(1);
					break;
				}
				case 0x48:	// less than (typed)
				{
					as_value& destination = env->top(1);
					if (destination.get_type() == as_value::STRING)
					{
						// SwiffPlayer.dll 0x10093AE2/0x10093AEA: right before left.
						const tu_string& right = env->top(0).to_tu_string();
						const tu_string& left = destination.to_tu_string();
						destination.set_bool(left < right);
					}
					else
					{
						destination.set_bool(destination < env->top(0));
					}
					env->drop(1);
					break;
				}
				case 0x49:	// equal (typed)
				{
					// SwiffPlayer.dll shares 0x100927AA with opcode 0x0E.
					as_value& left = env->top(1);
					left.set_bool(left == env->top(0));
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
					env->top(0).convert_to_string();
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
					// SwiffPlayer.dll 0x10093C3C-0x10093C4A: a null script-function
					// receiver survives unchanged; only its member-name operand is dropped.
					if (env->top(1).get_type() == as_value::AS_FUNCTION
					    && env->top(1).to_as_function() == NULL)
					{
						env->drop(1);
						break;
					}
                                        as_object_interface*    obj = env->top(1).to_object();

                                        // Special case: String has a member "length"
					// SwiffPlayer.dll 0x10093CEF: an unknown primitive-string
					// member leaves the string operand unchanged.
                                        if (obj == NULL && env->top(1).get_type() == as_value::STRING)
					{
						if (env->top(0).to_tu_stringi() == "length")
						{
							// SwiffPlayer.dll 0x10093D23 -> 0x1008B100 -> 0x1006E490
							// counts UTF-8 characters, unlike opcode 0x14's byte length.
							int len = env->top(1).to_tu_string().utf8_length();
							env->top(1).set_int(len);
						}
					}
                                        else
					{
						env->top(1).set_undefined();
						// int	nargs = (int) env->top(1).to_number();
						if (obj) {
							// SwiffPlayer.dll 0x10093D43/0x10093D87: get into a
							// temporary, then reacquire the operand after the accessor call.
							as_value member_value;
							obj->get_member(env->top(0).to_tu_string(), &member_value);
							env->top(1) = member_value;
							// 0x10093DAD/0x10093DC5 logs the temporary result only.
							IF_VERBOSE_ACTION(log_msg("-- get member: %s\n",
								member_value.to_tu_string().c_str()));
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
					// SwiffPlayer.dll 0x10093DEA-0x10093E3B dispatches OBJECT and
					// AS_FUNCTION separately; the function arm has no null-pointer guard.
					as_object_interface* obj = NULL;
					if (env->top(2).get_type() == as_value::OBJECT)
						obj = env->top(2).m_object_value;
					else if (env->top(2).get_type() == as_value::AS_FUNCTION)
						obj = env->top(2).to_as_function()->m_properties;
					if (obj)
					{
						// 0x10093DFD caches the value before converting the name.
						const as_value& value = env->top(0);
						const tu_string& name = env->top(1).to_tu_string();
						obj->set_member(name, value);
					}
					// SwiffPlayer.dll 0x10093E3E-0x10093EA8 also logs invalid
					// receivers, converting value, name, then receiver in that order.
					IF_VERBOSE_ACTION(
						const char* value = env->top(0).to_tu_string().c_str();
						const char* name = env->top(1).to_tu_string().c_str();
						const char* receiver = env->top(2).to_tu_string().c_str();
						log_msg("-- set member: %s.%s=%s\n", receiver, name, value));
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
					// SwiffPlayer.dll 0x10093F62-0x10093FCE: capture the OBJECT
					// receiver before arity conversion, then borrow the name string.
					as_object_interface*	obj = env->top(1).get_type() == as_value::OBJECT
						? env->top(1).m_object_value : NULL;
					int	nargs = bf2x87::toIntTruncate(env->top(2).to_number());
					as_value	result;
					const tu_string&	method_name = env->top(0).to_tu_string();
					s_call_super = NULL;
					// SwiffPlayer.dll 0x10094113 compares the converted name with
					// the literal "undefined"; the direct call keeps the current receiver.
					const bool direct_call = method_name == "undefined";
					if (!obj && env->top(1).get_type() == as_value::AS_FUNCTION)
					{
						as_as_function* receiver = env->top(1).to_as_function();
						if (direct_call)
						{
							// SwiffPlayer.dll 0x10094120-0x10094141: copy the operand and
							// call only a non-null script function (super() of a class whose
							// superclass is not a script function runs nothing).
							as_value function = env->top(1);
							if (function.to_as_function())
							{
								// 0x10094147-0x100941DF: the next super context comes from
								// that function's prototype when a current receiver exists.
								as_object_interface* proto = receiver->m_properties->m_instance_prototype;
								if (proto && env->m_this_object)
								{
									as_value constructor;
									if (proto->get_member("__constructor__", &constructor))
										s_call_super = constructor.to_as_function();
								}
								result = call_method(function, env, env->m_this_object, nargs, env->get_top_index() - 3);
							}
						}
						else
						{
							// SwiffPlayer.dll 0x1009423A-0x10094250: member lookup on the
							// function object itself, without a null-function guard.
							as_value function;
							if (receiver->m_properties->get_member(method_name, &function))
							{
								if (function.get_type() == as_value::C_FUNCTION
								    || function.get_type() == as_value::AS_FUNCTION)
								{
									// 0x100942BF: the call keeps the current receiver.
									result = call_method(function, env, env->m_this_object, nargs, env->get_top_index() - 3);
								}
								else
								{
									// 0x1009426B-0x100942A7 gates the error on verbose actions.
									IF_VERBOSE_ACTION(log_error("error: call_method: '%s' is not a method\n",
										  method_name.c_str()));
								}
							}
						}
					}
					else if (obj)
					{
						as_value	method;
						if (obj->get_member(method_name, &method))
						{
							if (method.get_type() != as_value::C_FUNCTION
							    && method.get_type() != as_value::AS_FUNCTION)
							{
								// SwiffPlayer.dll 0x10094083/0x100940B7 gates errors.
								IF_VERBOSE_ACTION(log_error("error: call_method: '%s' is not a method\n",
									  method_name.c_str()));
							}
							else
							{
								result = call_method(
									method,
									env,
									obj,
									nargs,
									env->get_top_index() - 3);
							}
						}
						else
						{
							IF_VERBOSE_ACTION(log_error("error: call_method can't find method %s\n", method_name.c_str()));
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
							// SwiffPlayer.dll 0x1009433A-0x1009435B.
							IF_VERBOSE_ACTION(log_error("error: call_method '%s'op on invalid object.\n", method_name.c_str()));
						}
					}
					env->drop(nargs + 2);
					env->top(0) = result;
					break;
				}
				case 0x53:	// new method
				{
					// SwiffPlayer.dll 0x10094398-0x1009455C: consume name, receiver
					// and count first; failed lookup leaves the arguments and pushes no result.
					tu_string method_name;
					{
						// 0x10094398-0x100943BC: copy the converted name, then
						// release the popped value before fetching the receiver.
						as_value method_value = env->pop();
						method_name = method_value.to_tu_string();
					}
					as_object_interface* receiver;
					{
						// 0x100943C1-0x100943E1: the named-member form keeps
						// only a raw OBJECT pointer, releasing before arity conversion.
						as_value obj_val = env->pop();
						receiver = obj_val.get_type() == as_value::OBJECT ? obj_val.m_object_value : NULL;
					}
					int	nargs = bf2x87::toIntTruncate(env->pop().to_number());

					as_value	constructor;
					if (!receiver || !receiver->get_member(method_name.to_tu_stringi(), &constructor)) break;

					as_value	new_obj;
					if (constructor.get_type() == as_value::C_FUNCTION
					    || constructor.get_type() == as_value::AS_FUNCTION)
					{
						// SwiffPlayer.dll 0x100944D1 / 0x100944F5: a C function
						// leaves the script pointer null and still reads its prototype.
						as_as_function* ctor_as_func = constructor.to_as_function();
						as_object_interface* proto = ctor_as_func->m_properties->m_instance_prototype;
						smart_ptr<as_object>	new_obj_ptr(new as_object(proto));
						new_obj.set_as_object_interface(new_obj_ptr.get_ptr());
						// SwiffPlayer.dll 0x100944FF links the new object into the collector
						// list (slot +0xC) before the call. [CHOSEN, not recovered] owner
						// decision 2026-11-09 (issue #7): that list stays inactive.
						call_method(constructor, env, new_obj_ptr.get_ptr(), nargs, env->get_top_index());
					}
					else
					{
						IF_VERBOSE_ACTION(log_error("error: call_method: '%s' is not a method\n", method_name.c_str()));
						break;
					}

					env->drop(nargs);
					env->push(new_obj);
					break;
				}
				case 0x54:	// instance of
					// @@ TODO
					log_error("todo opcode: %02X\n", action_id);
					break;
				case 0x55:	// enumerate2
				{
					// SwiffPlayer.dll 0x1009457D-0x100945BD: save/pop the value,
					// push the sentinel, then dispatch +0x28 without a null guard.
					as_value variable = env->pop();
					as_object_interface* object = variable.to_object_raw();

					// The end of the enumeration
					// SwiffPlayer.dll 0x100945AC constructs the STRING "NULL" sentinel.
					env->push("NULL");

					//  the object's own enumerate (arrays: indices only).
					object->enumerate(env);

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
				{
					// SwiffPlayer.dll 0x1009470F retains the left destination
					// across the equality helper when the tags match.
					as_value& left = env->top(1);
					if (left.get_type() != env->top(0).get_type())
					{
						// Types don't match.
						left.set_bool(false);
						env->drop(1);
					}
					else
					{
						left.set_bool(left == env->top(0));
						env->drop(1);
					}
					break;
				}
				case 0x67:	// gt (typed)
				{
					if (env->top(1).get_type() == as_value::STRING)
					{
						// SwiffPlayer.dll 0x1009476D/0x10094775: right before left.
						as_value& destination = env->top(1);
						const tu_string& right = env->top(0).to_tu_string();
						const tu_string& left = destination.to_tu_string();
						destination.set_bool(left > right);
					}
					else
					{
						// SwiffPlayer.dll 0x100947AC/0x100947B6: left before right.
						const double left = env->top(1).to_number();
						const double right = env->top(0).to_number();
						env->top(1).set_bool(left > right);
					}
					env->drop(1);
					break;
				}
				case 0x68:	// string gt
				{
					// SwiffPlayer.dll 0x10094803/0x1009480B: right before left.
					as_value& destination = env->top(1);
					const tu_string& right = env->top(0).to_tu_string();
					const tu_string& left = destination.to_tu_string();
					destination.set_bool(left > right);
					env->drop(1);
					break;
				}

				case 0x69:	// extends  (ActionExtends)
				{
					// SwiffPlayer.dll 0x1009481C-0x100948F6: discard each popped
					// value before reading the next; neither subclass nor prototype is guarded.
					as_as_function* super_fn = env->pop().to_as_function();
					as_as_function* sub_fn = env->pop().to_as_function();
					as_object_interface* sub_proto = sub_fn->m_properties->m_instance_prototype;
					sub_proto->set_member("__constructor__", as_value(super_fn));
					// 0x100948AC/0x100948BA: a non-script superclass supplies OBJECT null.
					as_object_interface* super_proto = super_fn
						? super_fn->m_properties->m_instance_prototype : NULL;
					// 0x100948CF reloads after the first virtual write, which can run script.
					sub_fn->m_properties->m_instance_prototype->set_member("__proto__", as_value(super_proto));
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

					break;
				}

				case 0x87:	// store_register
				{
					int	reg = m_buffer[pc + 3];
					// Save top of stack in specified register.
					//  the environment's bank, for timeline code and function
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
					// SwiffPlayer.dll 0x10094A78 reports the unsupported action.
					log_error("todo opcode: %02X\n", action_id);
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
					// SwiffPlayer.dll 0x10094AAD: queue the label, then stop.
					// [CHOSEN, not recovered] S3 bridge: the native character
					// interface exposes the separate queued-label virtual.
					env->get_target()->cast_to_character()->queue_goto_labeled_frame(frame_label);
					env->get_target()->set_play_state(movie::STOP);
					break;
				}

				case 0x8D:	// wait for frame expression (?)
				{
					// SwiffPlayer.dll's extended-opcode table selects the default arm.
					break;
				}

				case 0x8E:	// function2
				{
					// SwiffPlayer.dll 0x10094AEA: declarations use the caller's environment.
					as_as_function*	func = new as_as_function(this, NULL, next_pc, array<with_stack_entry>());
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

					// Get the register assignments and names of the arguments.
					for (int n = 0; n < nargs; n++)
					{
						int	arg_register = m_buffer[i];
						i++;

						// @@ security: watch out for possible missing terminator here!
						// SwiffPlayer.dll 0x10094B72-0x10094BD4 parses a temporary
						// name but stores only the register number, even for register zero.
						const tu_string argument_name((const char*) &m_buffer[i]);
						i += argument_name.length() + 1;
						// [CHOSEN, not recovered] S3 bridge: native function classes
						// share an argument record; function2 does not retain its name.
						func->add_arg(arg_register, "");
					}

					// SwiffPlayer.dll 0x10094BDD installs preloads after parsing
					// and discarding every argument name, before reading body length.
					func->set_function2_flags(flags);

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
						// SwiffPlayer.dll 0x10094DF5: function2 publishes on the movie.
						env->get_target()->set_member(name, function_value);
					}
					// 0x10094E0F: function2 pushes named functions too.
					env->push_val(function_value);

					break;
				}

				case 0x94:	// with
				{
					if (with_stack.size() < 8)
					{
 						int	block_length = m_buffer[pc + 3] | (m_buffer[pc + 4] << 8);
 						int	block_end = next_pc + block_length;
						// SwiffPlayer.dll 0x10094E82: only OBJECT supplies a scope;
						// script functions do not expose their property bag here.
						as_object_interface*	with_obj = env->top(0).get_type() == as_value::OBJECT
							? env->top(0).m_object_value : NULL;
						// 0x10094E8E-0x10094EA4 appends a borrowed scope record.
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
							// SwiffPlayer.dll 0x10094F66/0x100098C0: STRING "NULL".
							env->push("NULL");

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
							//  the environment's bank for all code - the
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
							// SwiffPlayer.dll 0x100950F1-0x1009511A builds a dword
							// from unsigned bytes, then passes it as a signed integer.
							const Uint32 bits = Uint32(m_buffer[3 + i])
								| (Uint32(m_buffer[3 + i + 1]) << 8)
								| (Uint32(m_buffer[3 + i + 2]) << 16)
								| (Uint32(m_buffer[3 + i + 3]) << 24);
							Sint32 val;
							memcpy(&val, &bits, sizeof(val));
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
								IF_VERBOSE_ACTION(log_msg("-------------- pushed 0 @@\n"));
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

								IF_VERBOSE_ACTION(log_msg("-------------- pushed 0 @@"));
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
					env->drop(2);
					break;
				}

				case 0x9B:	// declare function
				{
					// SwiffPlayer.dll 0x100952C9: no defining environment or with capture.
					as_as_function*	func = new as_as_function(this, NULL, next_pc, array<with_stack_entry>());

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
						func->add_arg((const char*) &m_buffer[i]);
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

					else
					{
						// 0x100953D8: the anonymous branch alone pushes the function.
						env->push_val(function_value);
					}

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
					// SwiffPlayer.dll 0x1009549A: strings use the immediate label
					// entry; every other type is numeric. Set play state even if
					// label lookup fails, without trying the string as a number.
					unsigned char	play_flag = m_buffer[pc + 3];
					movie::play_state	state = play_flag ? movie::PLAY : movie::STOP;
					movie* target = env->get_target();
					if (env->top(0).get_type() == as_value::STRING)
					{
						target->goto_labeled_frame(env->top(0).to_string());
					}
					else
					{
						target->apply_goto_frame(bf2x87::toIntTruncate(env->top(0).to_number()));
					}
					env->get_target()->set_play_state(state);
					env->drop(1);

					break;
				}
				
				}
				pc = next_pc;
			}
		}

		env->set_target(original_target);
		s_current_environment = previous_environment;
		// SwiffPlayer.dll 0x10095541: falling off the buffer and RETURN restore depth.
		if (env->m_stack.size() != original_stack_size)
		{
			env->m_stack.resize(original_stack_size);
		}
	}


	//
	// as_value -- ActionScript value type
	//

	
	as_value::as_value(as_object_interface* obj)
		:
		m_type(OBJECT),
		m_object_value(obj)
	{
		// SwiffPlayer.dll 0x1008AC50: OBJECT tag and nullable object payload.
		// [CHOSEN, not recovered] S3 bridge: native objects use one refcount interface.
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
		// SwiffPlayer.dll 0x10080E70: type 6 and a conditional function retain.
		if (m_as_function_value)
		{
			m_as_function_value->add_ref();
		}
	}


	const char*	as_value::to_string() const
	// Conversion to string.
	// SwiffPlayer.dll 0x1008D0F0.
	{
		return to_tu_string().c_str();
	}


	const tu_stringi&	as_value::to_tu_stringi() const
	{
		// SwiffPlayer.dll 0x1008D110 shares the to_tu_string entry.
		return reinterpret_cast<const tu_stringi&>(to_tu_string());
	}


	const tu_string&	as_value::to_tu_string() const
	// Conversion to const tu_string&.
	{
		// SwiffPlayer.dll 0x1008B430; the native CRT/ownership adapters stay separate.
		if (m_type == STRING) { /* don't need to do anything */ }
		else if (m_type == NUMBER)
		{
			// @@ Moock says if value is a NAN, then result is "NaN"
			// INF goes to "Infinity"
			// -INF goes to "-Infinity"
			// 0x1008B459: _gcvt(value, 30), followed by removal of a final period.
			static char buffer[128];
#if defined(_WIN32)
			_gcvt(m_number_value, 30, buffer);
#else
			// [CHOSEN, not recovered] POSIX CRT bridge: Darwin has no gcvt.
			// MSVCR71 digit/locale equivalence, including nonfinite spelling, is open.
			_snprintf(buffer, sizeof(buffer), "%.30g", m_number_value);
#endif
			const size_t length = strlen(buffer);
			if (length && buffer[length - 1] == '.') buffer[length - 1] = 0;
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

				//  The shipped to_tu_string (gameswf_as_value_to_tu_string,
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
			_snprintf(buffer, 50, "<c_function 0x%zX>", (size_t) (uintptr_t) m_c_function_value);
			m_string_value = buffer;
		}
		else if (m_type == AS_FUNCTION)
		{
			char buffer[50];
			_snprintf(buffer, 50, "<as_function 0x%zX>", (size_t) (uintptr_t) m_as_function_value);
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
	// SwiffPlayer.dll 0x1008B570.
	{
		if (m_type == STRING)
		{
			const char* text = m_string_value.c_str();
			if (text[0] == '0' && text[1] == 'x')
			{
				m_number_value = 0.0;
				for (text += 2; *text; ++text)
				{
					// Retail multiplies before validating the next digit.
					m_number_value *= 16.0;
					int digit;
					if (*text >= '0' && *text <= '9') digit = *text - '0';
					else if (*text >= 'a' && *text <= 'f') digit = *text - 'a' + 10;
					else if (*text >= 'A' && *text <= 'F') digit = *text - 'A' + 10;
					else break;
					m_number_value += digit;
				}
			}
			else
			{
				m_number_value = atof(text);
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
			return atof(m_object_value->get_text_value());
		}
		else
		{
			return 0.0;
		}
	}


	bool	as_value::to_bool() const
	// Conversion to boolean.
	// SwiffPlayer.dll 0x1008B6B0; NaN remains truthy.
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
			return false;
		}
	}

	
	as_object_interface* as_value::to_object_raw() const
	{
		// SwiffPlayer.dll 0x1008A2C0 accepts only type 4, including a null payload.
		return m_type == OBJECT ? m_object_value : NULL;
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
			// [CHOSEN, not recovered] S3 bridge: retained native consumers
			// access function properties through an object adapter. Recovered type-4
			// extraction uses to_object_raw, never this additional conversion.
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
		// SwiffPlayer.dll 0x1008A2D0: no coercion or retained ownership.
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
		// SwiffPlayer.dll 0x1008A2E0: no coercion or retained ownership.
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
	// SwiffPlayer.dll 0x1008B770.
	{
		set_double(to_number());
	}


	void	as_value::convert_to_string()
	// Force type to string.
	{
		// SwiffPlayer.dll 0x1008B7C0 releases the old reference before conversion.
		drop_refs();
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
		// SwiffPlayer.dll 0x1008ACA0 releases even when assigning the same pointer.
		// [CHOSEN, not recovered] S3 bridge: native objects share one refcount interface.
		drop_refs();
		m_type = OBJECT;
		m_object_value = obj;
		if (m_object_value)
		{
			m_object_value->add_ref();
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
	// SwiffPlayer.dll 0x1008B800, arm by arm on
	// THIS operand's type: a string compares with the other's string conversion; a number with the
	// other's to_number; a boolean by both sides' to_bool; an OBJECT by the two string conversions
	// (a text-less object converts to "<object 0x%X>" with its own address - see to_tu_string - so
	// distinct clips differ and the same clip matches); a null object compares an undefined value's
	// string ("undefined") with the other's; everything else (undefined, both function types) is
	// equal exactly when the types match. Stock 2006 answered `m_type == v.m_type` for objects, so
	// every clip equalled every other: mainMenu.swf's Radio.toggleCheck skipped all its siblings
	// as `other != this` was never true, and the difficulty radios could all be lit at once.
	// [CHOSEN, not recovered] S3 bridge: the 2006 NULLTYPE maps to retail's type 0
	// for comparison. Dispatch still depends only on the left operand's type.
	{
		if (v.m_type == NULLTYPE) return *this == as_value();
		if (m_type == STRING)
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
			const type left = m_type == NULLTYPE ? UNDEFINED : m_type;
			const type right = v.m_type == NULLTYPE ? UNDEFINED : v.m_type;
			return left == right;
		}
	}

	
	bool	as_value::operator!=(const as_value& v) const
	// Return true if operands are not equal.
	{
		return ! (*this == v);
	}

	// SwiffPlayer.dll 0x1008C020: convert the left operand before the right.
	bool as_value::operator<(const as_value& v) const
	{
		const double left = to_number();
		const double right = v.to_number();
		return left < right;
	}

	// SwiffPlayer.dll 0x1008C060: ADD converts the right operand first.
	void as_value::operator+=(const as_value& v)
	{
		const double right = v.to_number();
		const double left = to_number();
		set_double(left + right);
	}

	// SwiffPlayer.dll 0x1008C0C0: SUB converts the left operand first.
	void as_value::operator-=(const as_value& v)
	{
		const double left = to_number();
		const double right = v.to_number();
		set_double(left - right);
	}

	// SwiffPlayer.dll 0x1008C120: MUL converts the right operand first.
	void as_value::operator*=(const as_value& v)
	{
		const double right = v.to_number();
		const double left = to_number();
		set_double(left * right);
	}

	// SwiffPlayer.dll 0x1008C180: DIV converts the left operand first, with no zero guard.
	void as_value::operator/=(const as_value& v)
	{
		const double left = to_number();
		const double right = v.to_number();
		set_double(left / right);
	}

	
	void	as_value::string_concat(const tu_string& str)
	// Sets *this to this string plus the given string.
	{
		// SwiffPlayer.dll 0x1008B8F0: release, convert, set type, append.
		drop_refs();
		to_tu_string();	// make sure our m_string_value is initialized
		m_type = STRING;
		m_string_value += str;
	}

	// SwiffPlayer.dll 0x1008C1E0: convert the right operand first, using signed FISTP.
	void as_value::operator&=(const as_value& v)
	{
		const int right = bf2x87::toIntTruncate(v.to_number());
		const int left = bf2x87::toIntTruncate(to_number());
		set_int(left & right);
	}

	// SwiffPlayer.dll 0x1008C240.
	void as_value::operator|=(const as_value& v)
	{
		const int right = bf2x87::toIntTruncate(v.to_number());
		const int left = bf2x87::toIntTruncate(to_number());
		set_int(left | right);
	}

	// SwiffPlayer.dll 0x1008C2A0.
	void as_value::operator^=(const as_value& v)
	{
		const int right = bf2x87::toIntTruncate(v.to_number());
		const int left = bf2x87::toIntTruncate(to_number());
		set_int(left ^ right);
	}

	// SwiffPlayer.dll 0x1008C300: x86 masks the shift count to five bits.
	void as_value::shl(const as_value& v)
	{
		const unsigned int count = bf2x87::toIntTruncate(v.to_number()) & 31;
		const Uint32 left = static_cast<Uint32>(bf2x87::toIntTruncate(to_number()));
		const Uint32 bits = left << count;
		int result;
		memcpy(&result, &bits, sizeof(result));
		set_int(result);
	}

	// SwiffPlayer.dll 0x1008C360: SAR sign-extends the signed dword.
	void as_value::asr(const as_value& v)
	{
		const unsigned int count = bf2x87::toIntTruncate(v.to_number()) & 31;
		const int left = bf2x87::toIntTruncate(to_number());
		Uint32 bits = static_cast<Uint32>(left) >> count;
		if (left < 0 && count != 0) bits |= ~Uint32(0) << (32 - count);
		int result;
		memcpy(&result, &bits, sizeof(result));
		set_int(result);
	}

	// SwiffPlayer.dll 0x1008C3C0: SHR returns the unsigned dword as a NUMBER.
	void as_value::lsr(const as_value& v)
	{
		const unsigned int count = bf2x87::toIntTruncate(v.to_number()) & 31;
		const Uint32 left = static_cast<Uint32>(bf2x87::toIntTruncate(to_number()));
		set_double(static_cast<double>(left >> count));
	}

	void	as_value::drop_refs()
	// Drop any ref counts we have; this happens prior to changing our value.
	{
		// SwiffPlayer.dll 0x1008A2F0 leaves the type and payload bits untouched.
		if (m_type == AS_FUNCTION)
		{
			if (m_as_function_value)
			{
				m_as_function_value->drop_ref();
			}
		}
		else if (m_type == OBJECT)
		{
			// [CHOSEN, not recovered] S3 bridge: native interfaces own their object
			// references directly; retail's separate ownership-discriminator byte is absent.
			if (m_object_value)
			{
				m_object_value->drop_ref();
			}
		}
	}


	//
	// as_environment
	//


	as_value	as_environment::get_variable(const tu_string& varname, const array<with_stack_entry>& with_stack,
		as_object_interface** receiver) const
	// Return the value of the given var, if it's defined.
	// SwiffPlayer.dll 0x10091EF0 leaves receiver untouched for a parsed path.
	{
		// Path lookup rigamarole.
		tu_string	path;
		tu_string	var;
		if (parse_path(varname, &path, &var))
		{
			as_object_interface*	target = find_object_path(path, NULL);
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
			return this->get_variable_raw(varname, with_stack, receiver);
		}
	}


	as_value	as_environment::get_variable_raw(
		const tu_string& varname,
		const array<with_stack_entry>& with_stack,
		as_object_interface** receiver) const
	// varname must be a plain variable name; no path parsing.
	// SwiffPlayer.dll 0x100914C0: with, locals, this, target, modules, root, global.
	{
		as_value	val;

		// Check the with-stack.
		for (int i = with_stack.size() - 1; i >= 0; i--)
		{
			as_object_interface*	obj = with_stack[i].m_object;
			if (obj && obj->get_member(varname, &val))
			{
				// SwiffPlayer.dll 0x1009153B writes this even for a non-function.
				if (receiver) *receiver = obj;
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
			// the bound this-object of the running function, else the target movie
			val.set_as_object_interface(m_this_object ? m_this_object : static_cast<as_object_interface*>(m_target));
			return val;
		}

		// the bound this-object's members come before the target movie's -
		// the shipped get_variable_raw (sub_100914C0) consults env+8212 first.
		// SwiffPlayer.dll 0x10091652 repeats the target lookup even when it is this.
		if (m_this_object && m_this_object->get_member(varname, &val))
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
			// SwiffPlayer.dll 0x1009182C: return the published root as OBJECT.
			return as_value(get_global_root_movie());
		}
		if (s_modules.get(varname, &val))
		{
			return val;
		}
		// SwiffPlayer.dll 0x100916EA / 0x10091708: the receiver output uses
		// a second root read, since the member lookup can replace the publication.
		if (get_global_root_movie()->get_member(varname, &val))
		{
			if (receiver) *receiver = get_global_root_movie();
			return val;
		}
		as_value global;
		if (s_modules.get("_global", &global) && global.to_object()->get_member(varname, &val))
			return val;
	
		// Fallback.
		IF_VERBOSE_ACTION(log_msg("get_variable_raw(\"%s\") failed, returning UNDEFINED.\n", varname.c_str()));
		return as_value();
	}


	void	as_environment::set_variable(
		const tu_string& varname,
		const as_value& val,
		const array<with_stack_entry>& with_stack)
	// Given a path to variable, set its value.
	// SwiffPlayer.dll 0x10092060: a failed path leaves storage untouched.
	{
		IF_VERBOSE_ACTION(log_msg("-------------- %s = %s\n", varname.c_str(), val.to_string()));//xxxxxxxxxx

		// Path lookup rigamarole.
		tu_string	path;
		tu_string	var;
		if (parse_path(varname, &path, &var))
		{
			// through objects too - see find_object_path.
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
	// SwiffPlayer.dll 0x1008DA00: with members, then locals, then this/target.
	{
		// Check the with-stack.
		for (int i = with_stack.size() - 1; i >= 0; i--)
		{
			as_object_interface*	obj = with_stack[i].m_object;
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

		if (varname == "this")
		{
			if (val.get_type() == as_value::OBJECT) m_this_object = val.to_object();
			return;
		}

		assert(m_target);

		m_target->set_member(varname, val);
	}


	void	as_environment::set_local(const tu_string& varname, const as_value& val)
	// Set/initialize the value of the local variable.
	// SwiffPlayer.dll 0x1008DB70: timeline locals use the same storage as function locals.
	{
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
	// SwiffPlayer.dll 0x1008DC20.
	{
		assert(varname.length() > 0);
		m_local_frames.push_back(frame_slot(varname, val));
	}


	void	as_environment::declare_local(const tu_string& varname)
	// Create the specified local var if it doesn't exist already.
	// SwiffPlayer.dll 0x1008DCF0.
	{
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
	// SwiffPlayer.dll 0x10091890: locals precede the environment's member hash.
	{
		int index = find_local(tu_string(varname.c_str()));
		if (index >= 0)
		{
			*val = m_local_frames[index].m_value;
			return true;
		}
		return m_variables.get(varname, val);
	}


	void	as_environment::set_member(const tu_stringi& varname, const as_value& val)
	// SwiffPlayer.dll 0x10092130: update a local before creating a hash member.
	{
		int index = find_local(tu_string(varname.c_str()));
		if (index >= 0)
		{
			m_local_frames[index].m_value = val;
			return;
		}
		m_variables.set(varname, val);
	}

	as_value*	as_environment::register_ptr(int reg)
	//  The environment's register `reg`. The shipped as_environment holds
	// 256 of them inline (ctor 0x10077D00 constructs 256 undefined values at +0x10) and every
	// register operand is a byte, so any operand names a live slot; the bank is allocated here on
	// first use instead, which reads the same (an untouched register is undefined). Numbering is
	// direct from r0 - the zero-based numbering #43 introduced for the old per-call stack.
	//  ...in the bank register_bank() names: the calling chain's while one
	// of this environment's functions runs for another environment - see as_as_function::operator().
	{
		as_environment*	bank = register_bank();
		if (bank->m_registers.size() == 0)
		{
			bank->m_registers.resize(256);
		}
		return &bank->m_registers[reg & 0xFF];
	}


	void	as_environment::release_values(array<as_value>* held)
	// [CHOSEN, not recovered] issue #7: movie teardown, see release_script_objects. Only this
	// environment's own storage is emptied; m_target, m_this_object, m_super_ctor and
	// m_register_env hold no references.
	{
		for (int i = 0; i < m_stack.size(); i++)
		{
			hold_script_value(held, m_stack[i]);
		}
		m_stack.resize(0);
		for (int i = 0; i < m_registers.size(); i++)
		{
			hold_script_value(held, m_registers[i]);
		}
		m_registers.resize(0);
		for (stringi_hash<as_value>::iterator it = m_variables.begin(); it != m_variables.end(); ++it)
		{
			hold_script_value(held, it->second);
		}
		m_variables.clear();
		for (int i = 0; i < m_local_frames.size(); i++)
		{
			hold_script_value(held, m_local_frames[i].m_value);
		}
		m_local_frames.resize(0);
	}


	// [CHOSEN, not recovered] issue #7: the live script objects, by address. Allocated on first
	// use and never freed, so objects destroyed during static destruction still find it.
	static std::unordered_set<as_object_interface*>*	s_script_objects = NULL;
	// SwiffPlayer.dll 0x100F9350; source-level name [INFERRED]. The native
	// registry remains issue #7's ownership bridge, separate from the retail sweep.
	static Uint32 s_common_live_count = 0;
	// SwiffPlayer.dll 0x100F935C; [INFERRED] source-level name. The retail
	// collector's list is separate from issue #7's native ownership registry.
	static as_object_interface* s_gc_list_head = NULL;

	void	register_script_object(as_object_interface* obj)
	{
		if (s_script_objects == NULL)
		{
			s_script_objects = new std::unordered_set<as_object_interface*>;
		}
		s_script_objects->insert(obj);
		// Inlined common construction in 0x1007EC60 / 0x1009A190 increments
		// before the derived character/object initializes its own members.
		++s_common_live_count;
	}

	void	unregister_script_object(as_object_interface* obj)
	{
		if (s_script_objects != NULL)
		{
			s_script_objects->erase(obj);
		}
		// SwiffPlayer.dll 0x1007DFD2..0x1007E001, inlined in text destruction
		// at 0x1008A032..0x1008A061: next, previous, head, then live count.
		if (obj->m_gc_next) obj->m_gc_next->m_gc_previous = obj->m_gc_previous;
		if (obj->m_gc_previous) obj->m_gc_previous->m_gc_next = obj->m_gc_next;
		if (s_gc_list_head == obj) s_gc_list_head = obj->m_gc_next;
		--s_common_live_count;
	}


	void	release_script_objects()
	// [CHOSEN, not recovered] issue #7 (https://github.com/B2BF/OpenBF2/issues/7). A movie's clips
	// and objects reference each other from script state - `this` and `_parent` left in a register
	// bank, a clip stored in another clip's variable or in a class's static property, a prototype
	// and its constructor, a clip removed from its parent while its own scripts still reference
	// it - so dropping the movie's last reference freed nothing. `held` keeps every registered
	// object alive while the edges are cleared, so the registry does not change under the loop;
	// its destruction then drops the last references to objects that no longer reference each
	// other. Objects nobody counts (a zero reference count: s_sprite_builtins, stack temporaries)
	// are left alone.
	{
		// [CHOSEN, not recovered] S3 bridge: no borrowed constructor may outlive
		// the native script graph whose references are about to be cleared.
		s_call_super = NULL;
		// [CHOSEN, not recovered] S3 bridge to issue #7's last-player teardown:
		// discard globally borrowed timer receivers/environments before the graph.
		clear_interval_registry();
		release_library_classes();
		if (s_script_objects == NULL)
		{
			return;
		}
		array<as_value>	held;
		for (std::unordered_set<as_object_interface*>::iterator it = s_script_objects->begin(); it != s_script_objects->end(); ++it)
		{
			if ((*it)->get_ref_count() > 0)
			{
				held.push_back(as_value(*it));
			}
		}
		for (int i = 0, n = held.size(); i < n; i++)
		{
			as_object_interface*	obj = held[i].to_object();
			if (obj == NULL)
			{
				continue;
			}
			if (character* ch = obj->cast_to_character())
			{
				ch->release_script_values(&held);
			}
			else if (as_object* ao = obj->cast_to_as_object())
			{
				// [CHOSEN, not recovered] issue #7 / S3 bridge: XML factories
				// now publish native strong graph edges; clear them while held owns
				// every registered object, before ordinary members and accessors.
				release_xml_script_references(ao);
				// [CHOSEN, not recovered] issue #7: dense Array values also own script
				// references. Clear them while the registry snapshot holds every object.
				if (as_array_object* array = dynamic_cast<as_array_object*>(ao))
				{
					array->m_elements.clear();
				}
				ao->clear();
				ao->m_accessors.clear();
			}
		}
	}


	as_environment::~as_environment()
	{
		// SwiffPlayer.dll 0x100778B0: locals, members, registers 255..0, operands.
		m_local_frames.clear();
		m_variables.clear();
		// [CHOSEN, not recovered] S3 bridge: free the native bank allocation
		// after reverse value destruction; retail's bank is embedded storage.
		m_registers.clear_reverse();
		m_stack.clear();
	}

	int	as_environment::find_local(const tu_string& varname) const
	// Search the active frame for the named var; return its index
	// in the m_local_frames stack if found.
	// 
	// Otherwise return -1.
	{
		// SwiffPlayer.dll 0x1008D120 registers even with profiling disabled,
		// and always reads the ending clock and updates the counter.
		static profile_record counter("find_local");
		if (s_profiling) counter.begin();
		int result = -1;
		for (int i = m_local_frames.size() - 1; i >= 0; i--)
		{
			const frame_slot&	slot = m_local_frames[i];
			if (slot.m_name.length() == 0)
			{
				// End of local frame; stop looking.
				break;
			}
			else if (slot.m_name == varname)
			{
				// Found it.
				result = i;
				break;
			}
		}
		counter.end();
		return result;
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
		// SwiffPlayer.dll 0x1008B940: first colon, otherwise last dot; trim one slash.
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


	as_object_interface*	as_environment::find_target(const as_value& val) const
	// Resolve an object value or string path to its common ActionScript interface.
	{
		// SwiffPlayer.dll 0x100921D0 returns OBJECT directly, without a movie cast.
		if (val.get_type() == as_value::OBJECT)
		{
			return val.m_object_value;
		}
		else if (val.get_type() == as_value::STRING)
		{
			return find_object_path(val.to_tu_string(), NULL);
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
		// SwiffPlayer.dll 0x1008A320 skips paired dots before testing a delimiter.
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
		// [CHOSEN, not recovered] S3 bridge: retain the 2006 movie-returning API
		// while every string path uses the common retail walk at 0x10091960.
		as_object_interface* target = find_object_path(path, NULL);
		return target ? target->to_movie() : NULL;
	}


	// SwiffPlayer.dll 0x1000B3B0 resolves plain-object relative targets.
	// [CHOSEN, not recovered] S3 bridge: adapt retail's common virtual to the
	// native movie/get_member interfaces; sprite dispatch remains a separate recovery.
	static as_object_interface*	resolve_path_element(as_object_interface* env, const tu_string& name)
	{
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
		// SwiffPlayer.dll 0x10091960: empty paths return the target, even with start.
		as_object_interface*	env = start ? start : m_target;
		if (path.length() <= 0)
		{
			return m_target;
		}
		const char*	p = path.c_str();
		tu_string	subpart;

		if (*p == '/')
		{
			// Absolute path.  Start at the root.
			env = resolve_path_element(env, tu_string("_level0"));
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
			as_value global_value;
			as_object_interface* global = s_modules.get("_global", &global_value)
				? global_value.to_object() : NULL;
			if (global && (start ? start : m_target) != global)
			{
				return find_object_path(path, global);
			}
		}
		return env;
	}


	//
	// event_id
	//

	//  the shipped table (sub_1008BA90) spells onReleaseOutside, onRollOver,
	// onRollOut, onDragOver and onDragOut without stock 2006's underscores - a sprite's on_event looks
	// its member handler up by this name, so the stock spellings never found one - and ends in
	// onDoublePress.
	const tu_string&	event_id::get_function_name() const
	{
		// SwiffPlayer.dll 0x1008BA90: construct all 22 names in table order.
		static const tu_string retail_names[] =
		{
			"INVALID", "onPress", "onRelease", "onReleaseOutside", "onRollOver",
			"onRollOut", "onDragOver", "onDragOut", "onKeyPress", "onInitialize",
			"onLoad", "onUnload", "onEnterFrame", "onMouseDown", "onMouseUp",
			"onMouseMove", "onKeyDown", "onKeyUp", "onData", "reserved",
			"onConstruct", "onDoublePress"
		};
		assert(m_id > INVALID && m_id < EVENT_COUNT);
		return retail_names[m_id];
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
		// SwiffPlayer.dll 0x1008A350 and instruction metadata at 0x100EC9E8.
		enum arg_format {
			ARG_NONE = 0,
			ARG_STR,
			ARG_HEX,	// default hex dump, in case the format is unknown or unsupported
			ARG_U8,
			ARG_U16,
			ARG_S16,
			ARG_PUSH_DATA,
			ARG_DECL_DICT
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
			{ 0x69, "extends", ARG_NONE },
			
			{ 0x81, "goto_frame", ARG_U16 },
			{ 0x83, "get_url", ARG_STR },
			{ 0x87, "store_register", ARG_U8 },
			{ 0x88, "decl_dict", ARG_DECL_DICT },
			{ 0x8A, "wait_for_frame", ARG_HEX },
			{ 0x8B, "set_target", ARG_STR },
			{ 0x8C, "goto_frame_lbl", ARG_STR },
			{ 0x8D, "wait_for_fr_exp", ARG_HEX },
			{ 0x8E, "func2", ARG_HEX },
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
