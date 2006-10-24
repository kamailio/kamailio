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


#define L_ALERT -3
#define L_CRIT  -2
#define L_ERR   -1
#define L_DEFAULT 0
#define L_WARN   1
#define L_NOTICE 2
#define L_INFO   3
#define L_DBG    4

/* vars:*/

extern int debug;
extern int log_stderr;
extern int log_facility;
extern volatile int dprint_crit; /* protection against "simultaneous"
									printing from signal handlers */

#define DPRINT_NON_CRIT		(dprint_crit==0)
#define DPRINT_CRIT_ENTER	(dprint_crit++)
#define DPRINT_CRIT_EXIT	(dprint_crit--)

#define DPRINT_LEV	1
/* priority at which we log */
#define DPRINT_PRIO LOG_DEBUG


void dprint (char* format, ...);

int str2facility(char *s);

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
				if ((debug>=DPRINT_LEV) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr){ \
						dprint (__VA_ARGS__); \
					}else{ \
						syslog(DPRINT_LEV|log_facility,  __VA_ARGS__); \
					}\
					DPRINT_CRIT_EXIT; \
				} \
			}while(0)
	#else
			#define DPrint(fmt,args...) \
			do{ \
				if ((debug>=DPRINT_LEV) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr){ \
						dprint (fmt, ## args); \
					}else{ \
						syslog(DPRINT_LEV|log_facility, fmt, ## args); \
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
				if ((debug>=(lev)) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr) dprint (__VA_ARGS__); \
					else { \
						switch(lev){ \
							case L_CRIT: \
								syslog(LOG_CRIT|log_facility, __VA_ARGS__); \
								break; \
							case L_ALERT: \
								syslog(LOG_ALERT|log_facility, __VA_ARGS__); \
								break; \
							case L_ERR: \
								syslog(LOG_ERR|log_facility, __VA_ARGS__); \
								break; \
							case L_WARN: \
								syslog(LOG_WARNING|log_facility, __VA_ARGS__);\
								break; \
							case L_NOTICE: \
								syslog(LOG_NOTICE|log_facility, __VA_ARGS__); \
								break; \
							case L_INFO: \
								syslog(LOG_INFO|log_facility, __VA_ARGS__); \
								break; \
							case L_DBG: \
								syslog(LOG_DEBUG|log_facility, __VA_ARGS__); \
								break; \
						} \
					} \
					DPRINT_CRIT_EXIT; \
				} \
			}while(0)
	#else
		#define LOG(lev, fmt, args...) \
			do { \
				if ((debug>=(lev)) && DPRINT_NON_CRIT){ \
					DPRINT_CRIT_ENTER; \
					if (log_stderr) dprint (fmt, ## args); \
					else { \
						switch(lev){ \
							case L_CRIT: \
								syslog(LOG_CRIT|log_facility, fmt, ##args); \
								break; \
							case L_ALERT: \
								syslog(LOG_ALERT|log_facility, fmt, ##args); \
								break; \
							case L_ERR: \
								syslog(LOG_ERR|log_facility, fmt, ##args); \
								break; \
							case L_WARN: \
								syslog(LOG_WARNING|log_facility, fmt, ##args);\
								break; \
							case L_NOTICE: \
								syslog(LOG_NOTICE|log_facility, fmt, ##args); \
								break; \
							case L_INFO: \
								syslog(LOG_INFO|log_facility, fmt, ##args); \
								break; \
							case L_DBG: \
								syslog(LOG_DEBUG|log_facility, fmt, ##args); \
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
		#define DEBUG(...) DBG("DEBUG"            LOC_INFO __VA_ARGS__)
		#define ERR(...)  LOG(L_ERR, "ERROR: "    LOC_INFO __VA_ARGS__)
		#define WARN(...) LOG(L_WARN, "WARNING: " LOC_INFO __VA_ARGS__)
		#define INFO(...) LOG(L_INFO, "INFO: "    LOC_INFO __VA_ARGS__)
		#define BUG(...) LOG(L_CRIT, "BUG: "      LOC_INFO __VA_ARGS__)
#else
		#define DEBUG(fmt, args...) DBG("DEBUG "        LOC_INFO fmt, ## args)
		#define ERR(fmt, args...) LOG(L_ERR, "ERROR: "  LOC_INFO fmt, ## args)
		#define WARN(fmt, args...) LOG(L_WARN, "WARN: " LOC_INFO fmt, ## args)
		#define INFO(fmt, args...) LOG(L_INFO, "INFO: " LOC_INFO fmt, ## args)
		#define BUG(fmt, args...) LOG(L_CRIT, "BUG: "   LOC_INFO fmt, ## args)
#endif


#endif /* ifndef dprint_h */
