// gameswf_log.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Helpers for logging messages & errors.


#ifndef GAMESWF_LOG_H
#define GAMESWF_LOG_H


namespace gameswf
{
	// Printf-style interfaces.

#ifdef __GNUC__
	// use the following to catch errors: (only with gcc)
	void	log_msg(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
	void	log_error(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
#else	// not __GNUC__
	void	log_msg(const char* fmt, ...);
	void	log_error(const char* fmt, ...);
#endif	// not __GNUC__


#if !defined(_MSC_VER)
	// [TEMP] c-swf-disconnect display-list tracer, off unless
	// BF2_SWF_LIST_TRACE=1. One flushed stderr line per event, prefixed "[swf-list]"; a line
	// identical to the previous one is counted, not repeated. g_swf_mouse_pass_depth is the
	// nesting of sprite mouse passes, so a line can say whether it ran inside one.
	bool	swf_list_trace_on();
	void	swf_list_trace(const char* fmt, ...) __attribute__((format (printf, 1, 2)));
	extern int	g_swf_mouse_pass_depth;
#define SWF_LIST_TRACE(...) do { if (gameswf::swf_list_trace_on()) gameswf::swf_list_trace(__VA_ARGS__); } while (0)
#else
#define SWF_LIST_TRACE(...) do { } while (0)
#endif
}


#endif // GAMESWF_LOG_H


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
