/*
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file
 * @brief Kamailio core :: debug printing
 * @ingroup core
 * Module: @ref core
 */

#ifndef dprint_h
#define dprint_h

#include <assert.h>
#include <syslog.h>
#include <stdio.h> /* stderr, fprintf() */

#include "compiler_opt.h"
#include "cfg_core.h"


/** dicover the function name */
/* C >= 99 has __func__, older gcc versions have __FUNCTION__ */
#if __STDC_VERSION__ < 199901L
#	if __GNUC__ >= 2
#		define _FUNC_NAME_ __FUNCTION__
#		define _FUNC_SUFFIX_ "(): "
#	else
#		define _FUNC_NAME_ ""
#		define _FUNC_SUFFIX_ ""
#	endif
#else
#	define _FUNC_NAME_ __func__
#	define _FUNC_SUFFIX_ "(): "
#endif

#ifdef NO_DEBUG
#	ifdef MOD_NAME
#		define LOC_INFO		MOD_NAME ": "
#		define LOG_MNAME	MOD_NAME
#	else
#		define LOC_INFO		"<core>: "
#		define LOG_MNAME	"core"
#	endif
#else
#	define XCT2STR(i) #i
#	define CT2STR(l)  XCT2STR(l)
#
#	ifdef MOD_NAME
#		define LOC_INFO		MOD_NAME " [" __FILE__ ":" CT2STR(__LINE__) "]: "
#		define LOG_MNAME	MOD_NAME
#	else
#		define LOC_INFO		"<core> [" __FILE__ ":" CT2STR(__LINE__) "]: "
#		define LOG_MNAME	"core"
#	endif
#
#	ifdef NO_LOG
#		undef NO_LOG
#	endif
#endif /* NO_DEBUG */

#define LOG_MNAME_LEN		(sizeof(LOG_MNAME)-1)

/*
 * Log levels
 */
#define L_NPRL		-6 /* (L_MIN-1) to skip printing level prefix */
#define L_MIN		-5
#define L_ALERT		-5
#define L_BUG		-4
#define L_CRIT2		-3  /* like L_CRIT, but adds prefix */
#define L_CRIT  	-2  /* no prefix added */
#define L_ERR   	-1
#define L_WARN   	0
#define L_NOTICE 	1
#define L_INFO   	2
#define L_DBG    	3
#define L_MAX    	3
#define L_OFFSET   42 /* needs to be added and then substracted
                        because L_WARN may be confused with NULL pointer
                        (e.g. fixup_dbg_sip_msg) */

/** @brief This is the facility value used to indicate that the caller of the macro
 * did not override the facility. Value 0 (the defaul) is LOG_KERN on Linux
 */
#define DEFAULT_FACILITY 0

#define LOG_LEVEL2NAME(level)	(log_level_info[(level) - (L_ALERT)].name)
#define LOG2SYSLOG_LEVEL(level) \
	(log_level_info[(level) - (L_ALERT)].syslog_level)

/**
 * data fileds used for structured logging
 */
typedef struct ksr_logdata {
	/* next field are automatically set by log macro */
	int v_facility;
	int v_level;
	char *v_lname;
	const char *v_fname;
	int v_fline;
	const char *v_mname;
	const char *v_func;
	const char *v_locinfo;
	/* next field are __not__ automatically set by log macro */
	int v_pid;
	int v_pidx;
} ksr_logdata_t;

typedef void (*ksr_slog_f)(ksr_logdata_t*, const char*, ...);
void ksr_slog_init(char *ename);

extern ksr_slog_f _ksr_slog_func;

/** @brief my_pid(), process_no are from pt.h but we cannot \#include it here
   because of circular dependencies */
extern int process_no;
extern int my_pid(void);

/** @brief non-zero if logging to stderr instead to the syslog */
extern int log_stderr;

extern int log_color;
extern int log_cee;
extern char *log_prefix_fmt;
extern str *log_prefix_val;
extern int log_prefix_mode;
extern char *_km_log_engine_type;
extern char *_km_log_engine_data;

typedef void (*km_log_f)(int, const char *, ...);
extern km_log_f _km_log_func;

void km_log_func_set(km_log_f f);

/** @brief maps log levels to their string name and corresponding syslog level */

struct log_level_info {
 	char *name;
	int syslog_level;
};

/** @brief per process debug level handling */
int get_debug_level(char *mname, int mnlen);
int get_cfg_debug_level(void);
int get_debug_facility(char *mname, int mnlen);
void set_local_debug_level(int level);
void set_local_debug_facility(int facility);
void reset_local_debug_level(void);
void reset_local_debug_facility(void);
typedef int (*get_module_debug_level_f)(char *mname, int mnlen, int *mlevel);
typedef int (*get_module_debug_facility_f)(char *mname, int mnlen, int *mfacility);
void set_module_debug_level_cb(get_module_debug_level_f f);
void set_module_debug_facility_cb(get_module_debug_facility_f f);

#define is_printable(level) (get_debug_level(LOG_MNAME, LOG_MNAME_LEN)>=(level))
extern struct log_level_info log_level_info[];
extern char *log_name;
extern char *log_fqdn;

#ifndef NO_SIG_DEBUG
/** @brief protection against "simultaneous" printing from signal handlers */
extern volatile int dprint_crit;
#endif

int str2facility(char *s);
char* facility2str(int fl, int *len);

int log_facility_fixup(void *handle, str *gname, str *name, void **val);

void dprint_color(int level);
void dprint_color_reset(void);
void dprint_color_update(int level, char f, char b);
void dprint_init_colors(void);
void dprint_term_color(char f, char b, str *obuf);

void log_init(void);
void log_prefix_init(void);

#define LOGV_PREFIX_STR ((log_prefix_val)?log_prefix_val->s:"")
#define LOGV_PREFIX_LEN ((log_prefix_val)?log_prefix_val->len:0)

#define LOGV_FUNCNAME_STR(vfuncname) (((void*)vfuncname!=NULL)?vfuncname:"")
#define LOGV_FUNCSUFFIX_STR(vfuncname) (((void*)vfuncname!=NULL)?_FUNC_SUFFIX_:"")

/** @brief
 * General logging macros
 *
 * LOG_FF(level, prefix, fmt, ...) prints "printf"-formatted log message to
 * stderr (if `log_stderr' is non-zero) or to syslog.  Note that `fmt' must
 * be constant. `prefix' is added to the beginning of the message.
 *
 * LOG(level, fmt, ...) is same as LOG_FP() with LOC_INFO prefix.
 */
#ifdef NO_LOG

#	ifdef __SUNPRO_C
#		define LOG_FX(facility, level, lname, prefix, funcname, ...)
#		define LOG_FL(facility, level, lname, prefix, ...)
#		define LOG_FP(facility, level, prefix, ...)
#		define LOG_FN(facility, level, prefix, ...)
#		define LOG_FC(facility, level, ...)
#		define LOG_LN(level, lname, ...)
#		define LOG(level, fmt, ...)
#	else
#		define LOG_FX(facility, level, lname, prefix, funcname, fmt, args...)
#		define LOG_FL(facility, level, lname, prefix, fmt, args...)
#		define LOG_FP(facility, level, prefix, fmt, args...)
#		define LOG_FN(facility, level, prefix, fmt, args...)
#		define LOG_FC(facility, level, fmt, args...)
#		define LOG_LN(level, lname, fmt, args...)
#		define LOG(level, fmt, args...)
#	endif

#else

#	ifdef NO_SIG_DEBUG
#		define DPRINT_NON_CRIT		(1)
#		define DPRINT_CRIT_ENTER
#		define DPRINT_CRIT_EXIT
#	else
#		define DPRINT_NON_CRIT		(dprint_crit==0)
#		define DPRINT_CRIT_ENTER	(dprint_crit++)
#		define DPRINT_CRIT_EXIT		(dprint_crit--)
#	endif

#	ifdef __SUNPRO_C
#		define LOG_FX(facility, level, lname, prefix, funcname, fmt, ...) \
			do { \
				if (DPRINT_NON_CRIT \
						&& get_debug_level(LOG_MNAME, LOG_MNAME_LEN) >= (level)) { \
					int __llevel; \
					__llevel = ((level)<L_ALERT)?L_ALERT:(((level)>L_DBG)?L_DBG:level); \
					DPRINT_CRIT_ENTER; \
					if (unlikely(log_stderr)) { \
						if (unlikely(log_color)) dprint_color(__llevel); \
						fprintf(stderr, "%2d(%d) %s: %.*s%s%s%s" fmt, \
								process_no, my_pid(), \
								(lname)?(lname):LOG_LEVEL2NAME(__llevel), \
								LOGV_PREFIX_LEN, LOGV_PREFIX_STR, \
								(prefix), LOGV_FUNCNAME_STR(funcname), \
								LOGV_FUNCSUFFIX_STR(funcname), __VA_ARGS__); \
						if (unlikely(log_color)) dprint_color_reset(); \
					} else { \
						_km_log_func(LOG2SYSLOG_LEVEL(__llevel) | \
							    (((facility) != DEFAULT_FACILITY) ? \
								(facility) : \
								get_debug_facility(LOG_MNAME, LOG_MNAME_LEN)), \
								"%s: %.*s%s%s%s" fmt, \
								(lname)?(lname):LOG_LEVEL2NAME(__llevel), \
								LOGV_PREFIX_LEN, LOGV_PREFIX_STR, \
								(prefix), LOGV_FUNCNAME_STR(funcname), \
								LOGV_FUNCSUFFIX_STR(funcname), __VA_ARGS__); \
					} \
					DPRINT_CRIT_EXIT; \
				} \
			} while(0)

#		define LOG_FL(facility, level, lname, prefix, ...) \
			LOG_FX(facility, level, lname, prefix, _FUNC_NAME_, __VA_ARGS__, NULL)

#		define LOG_FN(facility, level, prefix, ...) \
			LOG_FX(facility, level, NULL, prefix, NULL, __VA_ARGS__, NULL)

#		define LOG_FP(facility, level, prefix, ...) \
			LOG_FL(facility, level, NULL, prefix, __VA_ARGS__, NULL)

#		define LOG_FC(facility, level, ...) \
			LOG_FP((facility), (level), LOC_INFO, __VA_ARGS__, NULL)
#		define LOG_LN(level, lname, ...) \
			LOG_FL(DEFAULT_FACILITY, (level), (lname), LOC_INFO, \
						__VA_ARGS__, NULL)
#		define LOG(level, ...) \
			LOG_FP(DEFAULT_FACILITY, (level), LOC_INFO, __VA_ARGS__, NULL)


#	else /* ! __SUNPRO_C */
#		define LOG_FX(facility, level, lname, prefix, funcname, fmt, args...) \
			do { \
				if (DPRINT_NON_CRIT \
						&& get_debug_level(LOG_MNAME, LOG_MNAME_LEN) >= (level) ) { \
					int __llevel; \
					__llevel = ((level)<L_ALERT)?L_ALERT:(((level)>L_DBG)?L_DBG:level); \
					DPRINT_CRIT_ENTER; \
					if (_ksr_slog_func) { /* structured logging */ \
						ksr_logdata_t __kld = {0}; \
						__kld.v_facility = LOG2SYSLOG_LEVEL(__llevel) | \
							   (((facility) != DEFAULT_FACILITY) ? \
								(facility) : \
								get_debug_facility(LOG_MNAME, LOG_MNAME_LEN)); \
						__kld.v_level = __llevel; \
						__kld.v_lname = (lname)?(lname):LOG_LEVEL2NAME(__llevel); \
						__kld.v_fname = __FILE__; \
						__kld.v_fline = __LINE__; \
						__kld.v_mname = LOG_MNAME; \
						__kld.v_func = LOGV_FUNCNAME_STR(funcname); \
						__kld.v_locinfo = prefix; \
						_ksr_slog_func(&__kld, fmt, ## args); \
					} else { /* classic logging */ \
						if (unlikely(log_stderr)) { \
							if (unlikely(log_color)) dprint_color(__llevel); \
							fprintf(stderr, "%2d(%d) %s: %.*s%s%s%s" fmt, \
								process_no, my_pid(), \
								(lname)?(lname):LOG_LEVEL2NAME(__llevel), \
								LOGV_PREFIX_LEN, LOGV_PREFIX_STR, \
								(prefix), LOGV_FUNCNAME_STR(funcname), \
								LOGV_FUNCSUFFIX_STR(funcname), ## args); \
							if (unlikely(log_color)) dprint_color_reset(); \
						} else { \
							_km_log_func(LOG2SYSLOG_LEVEL(__llevel) | \
							   (((facility) != DEFAULT_FACILITY) ? \
								(facility) : \
								get_debug_facility(LOG_MNAME, LOG_MNAME_LEN)), \
								"%s: %.*s%s%s%s" fmt, \
								(lname)?(lname):LOG_LEVEL2NAME(__llevel), \
								LOGV_PREFIX_LEN, LOGV_PREFIX_STR, \
								(prefix), LOGV_FUNCNAME_STR(funcname), \
								LOGV_FUNCSUFFIX_STR(funcname), ## args); \
						} \
					} \
					DPRINT_CRIT_EXIT; \
				} \
			} while(0)

#		define LOG_FL(facility, level, lname, prefix, fmt, args...) \
			LOG_FX(facility, level, lname, prefix, _FUNC_NAME_, fmt, ## args)

#		define LOG_FN(facility, level, prefix, fmt, args...) \
			LOG_FX(facility, level, NULL, prefix, NULL, fmt, ## args)

#		define LOG_FP(facility, level, prefix, fmt, args...) \
			LOG_FL(facility, level, NULL, prefix, fmt, ## args)

#		define LOG(level, fmt, args...) \
			LOG_FP(DEFAULT_FACILITY, (level), LOC_INFO, fmt, ## args)
#		define LOG_FC(facility, level, fmt, args...) \
			LOG_FP((facility), (level), LOC_INFO, fmt, ## args)
#		define LOG_LN(level, lname, fmt, args...) \
			LOG_FL(DEFAULT_FACILITY, (level), (lname), LOC_INFO, fmt, ## args)

#	endif /* __SUNPRO_C */
#endif /* NO_LOG */


/** @name SimpleLog
 * Simplier, prefered logging macros for constant log level
 */
/*@ { */
#ifdef __SUNPRO_C
#	define NPRL(...)   LOG(L_NPRL,  __VA_ARGS__)
#	define ALERT(...)  LOG(L_ALERT,  __VA_ARGS__)
#	define BUG(...)    LOG(L_BUG,   __VA_ARGS__)
#	define ERR(...)    LOG(L_ERR,    __VA_ARGS__)
#	define WARN(...)   LOG(L_WARN,   __VA_ARGS__)
#	define NOTICE(...) LOG(L_NOTICE, __VA_ARGS__)
#	define INFO(...)   LOG(L_INFO,   __VA_ARGS__)
#	define CRIT(...)    LOG(L_CRIT2,   __VA_ARGS__)

#	ifdef NO_DEBUG
#		define DBG(...)
#	else
#		define DBG(...)    LOG(L_DBG, __VA_ARGS__)
#	endif
/*@ } */

/* obsolete, do not use */
#	define DEBUG(...) DBG(__VA_ARGS__)

#else /* ! __SUNPRO_C */
#	define NPRL(fmt, args...)   LOG(L_NPRL,  fmt , ## args)
#	define ALERT(fmt, args...)  LOG(L_ALERT,  fmt , ## args)
#	define BUG(fmt, args...)    LOG(L_BUG,   fmt , ## args)
#	define ERR(fmt, args...)    LOG(L_ERR,    fmt , ## args)
#	define WARN(fmt, args...)   LOG(L_WARN,   fmt , ## args)
#	define NOTICE(fmt, args...) LOG(L_NOTICE, fmt , ## args)
#	define INFO(fmt, args...)   LOG(L_INFO,   fmt , ## args)
#	define CRIT(fmt, args...)   LOG(L_CRIT2,   fmt , ## args)

#	ifdef NO_DEBUG
#		define DBG(fmt, args...)
#	else
#		define DBG(fmt, args...)    LOG(L_DBG, fmt , ## args)
#	endif

/* obsolete, do not use */
#	define DEBUG(fmt, args...) DBG(fmt , ## args)

#endif /* __SUNPRO_C */


/* kamailio/openser compatibility */

#define LM_GEN1 LOG
#define LM_GEN2 LOG_FC
#define LM_NPRL NPRL
#define LM_ALERT ALERT
#define LM_CRIT  CRIT
#define LM_BUG  BUG
#define LM_ERR ERR
#define LM_WARN WARN
#define LM_NOTICE NOTICE
#define LM_INFO INFO
#define LM_DBG DEBUG

#endif /* !dprint_h */
