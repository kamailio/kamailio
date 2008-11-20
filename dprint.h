/*
 * $Id$
 *
 * Copyright (C) 2001-2003 FhG Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */



#ifndef dprint_h
#define dprint_h

#include <syslog.h>
#include "cfg_core.h"


#define L_ALERT -3
#define L_CRIT  -2
#define L_ERR   -1
#define L_DEFAULT 0
#define L_WARN   1
#define L_NOTICE 2
#define L_INFO   3
#define L_DBG    4

/* vars:*/

extern int log_stderr;
extern volatile int dprint_crit; /* protection against "simultaneous"
									printing from signal handlers */

#ifdef NO_SIG_DEBUG
#define DPRINT_NON_CRIT		(1)
#define DPRINT_CRIT_ENTER
#define DPRINT_CRIT_EXIT
#else
#define DPRINT_NON_CRIT		(dprint_crit==0)
#define DPRINT_CRIT_ENTER	(dprint_crit++)
#define DPRINT_CRIT_EXIT	(dprint_crit--)
#endif

#define DPRINT_LEV	1
/* priority at which we log */
#define DPRINT_PRIO LOG_DEBUG


void dprint (char* format, ...);

int str2facility(char *s);
int log_facility_fixup(void *handle, str *name, void **val);

/* C >= 99 has __func__, older gcc versions have __FUNCTION__ */
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
#  define _FUNC_NAME_ __FUNCTION__
# else
#  define _FUNC_NAME_ ""
# endif
#else
# define _FUNC_NAME_ __func__
#endif


#define XCT2STR(i) #i
#define CT2STR(l) XCT2STR(l)

#define LOC_INFO	__FILE__ ":" CT2STR(__LINE__) ": "


#define is_printable(level) (cfg_get(core, core_cfg, debug)>=(level))

#ifdef NO_DEBUG
	#ifdef __SUNPRO_C
		#define DPrint(...)
	#else
		#define DPrint(fmt, args...)
	#endif
#else
	#ifdef __SUNPRO_C
		#define DPrint( ...) \
			do{ \
				if ((cfg_get(core, core_cfg, debug)>=DPRINT_LEV) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr){ \
						dprint (__VA_ARGS__); \
					}else{ \
						syslog(DPRINT_LEV|cfg_get(core, core_cfg, log_facility), \
							__VA_ARGS__); \
					}\
					DPRINT_CRIT_EXIT; \
				} \
			}while(0)
	#else
			#define DPrint(fmt,args...) \
			do{ \
				if ((cfg_get(core, core_cfg, debug)>=DPRINT_LEV) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr){ \
						dprint (fmt, ## args); \
					}else{ \
						syslog(DPRINT_LEV|cfg_get(core, core_cfg, log_facility), \
							fmt, ## args); \
					}\
					DPRINT_CRIT_EXIT; \
				} \
			}while(0)
	#endif

#endif

#ifndef NO_DEBUG
	#undef NO_LOG
#endif

#ifdef NO_LOG
	#ifdef __SUNPRO_C
		#define LOG(lev, ...)
	#else
		#define LOG(lev, fmt, args...)
	#endif
#else
	#ifdef __SUNPRO_C
		#define LOG(lev, ...) \
			do { \
				if ((cfg_get(core, core_cfg, debug)>=(lev)) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr) dprint (__VA_ARGS__); \
					else { \
						switch(lev){ \
							case L_CRIT: \
								syslog(LOG_CRIT|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__); \
								break; \
							case L_ALERT: \
								syslog(LOG_ALERT|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__); \
								break; \
							case L_ERR: \
								syslog(LOG_ERR|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__); \
								break; \
							case L_WARN: \
								syslog(LOG_WARNING|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__);\
								break; \
							case L_NOTICE: \
								syslog(LOG_NOTICE|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__); \
								break; \
							case L_INFO: \
								syslog(LOG_INFO|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__); \
								break; \
							case L_DBG: \
								syslog(LOG_DEBUG|cfg_get(core, core_cfg, log_facility), \
									__VA_ARGS__); \
								break; \
						} \
					} \
					DPRINT_CRIT_EXIT; \
				} \
			}while(0)
	#else
		#define LOG(lev, fmt, args...) \
			do { \
				if ((cfg_get(core, core_cfg, debug)>=(lev)) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr) dprint (fmt, ## args); \
					else { \
						switch(lev){ \
							case L_CRIT: \
								syslog(LOG_CRIT|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args); \
								break; \
							case L_ALERT: \
								syslog(LOG_ALERT|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args); \
								break; \
							case L_ERR: \
								syslog(LOG_ERR|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args); \
								break; \
							case L_WARN: \
								syslog(LOG_WARNING|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args);\
								break; \
							case L_NOTICE: \
								syslog(LOG_NOTICE|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args); \
								break; \
							case L_INFO: \
								syslog(LOG_INFO|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args); \
								break; \
							case L_DBG: \
								syslog(LOG_DEBUG|cfg_get(core, core_cfg, log_facility), \
									fmt, ##args); \
								break; \
						} \
					} \
					DPRINT_CRIT_EXIT; \
				} \
			}while(0)
	#endif /*SUN_PRO_C*/
#endif


#ifdef NO_DEBUG
	#ifdef __SUNPRO_C
		#define DBG(...)
	#else
		#define DBG(fmt, args...)
	#endif
#else
	#ifdef __SUNPRO_C
		#define DBG(...) LOG(L_DBG, __VA_ARGS__)
	#else
		#define DBG(fmt, args...) LOG(L_DBG, fmt, ## args)
	#endif
#endif

#ifdef __SUNPRO_C
		#define DEBUG(...) DBG("DEBUG: "          LOC_INFO __VA_ARGS__)
		#define ERR(...)  LOG(L_ERR, "ERROR: "    LOC_INFO __VA_ARGS__)
		#define WARN(...) LOG(L_WARN, "WARNING: " LOC_INFO __VA_ARGS__)
		#define INFO(...) LOG(L_INFO, "INFO: "    LOC_INFO __VA_ARGS__)
		#define BUG(...) LOG(L_CRIT, "BUG: "      LOC_INFO __VA_ARGS__)
		#define NOTICE(...) LOG(L_NOTICE, "NOTICE: " LOC_INFO __VA_ARGS__)
		#define ALERT(...) LOG(L_ALERT, "ALERT: " LOC_INFO __VA_ARGS__)
		#define CRIT(...) LOG(L_CRIT, "CRITICAL: " LOC_INFO __VA_ARGS__)
#else
		#define DEBUG(fmt, args...) DBG("DEBUG: "       LOC_INFO fmt, ## args)
		#define ERR(fmt, args...) LOG(L_ERR, "ERROR: "  LOC_INFO fmt, ## args)
		#define WARN(fmt, args...) LOG(L_WARN, "WARN: " LOC_INFO fmt, ## args)
		#define INFO(fmt, args...) LOG(L_INFO, "INFO: " LOC_INFO fmt, ## args)
		#define BUG(fmt, args...) LOG(L_CRIT, "BUG: "   LOC_INFO fmt, ## args)
		#define NOTICE(fmt, args...) \
			LOG(L_NOTICE, "NOTICE: " LOC_INFO fmt, ## args)
		#define ALERT(fmt, args...) \
			LOG(L_ALERT, "ALERT: " LOC_INFO fmt, ## args)
		#define CRIT(fmt, args...) \
			LOG(L_CRIT, "CRITICAL: " LOC_INFO fmt, ## args)
#endif

/* kamailio/openser compatibility */

#define LM_GEN1 LOG

#define LM_ALERT ALERT
#define LM_CRIT  CRIT
#define LM_ERR ERR
#define LM_WARN WARN
#define LM_NOTICE
#define LM_INFO INFO
#define LM_DBG DEBUG


#endif /* ifndef dprint_h */
