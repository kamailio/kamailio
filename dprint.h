/*
 * $Id$
 *
 * Copyright (C) 2001-2003 Fhg Fokus
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
#define L_WARN   1
#define L_NOTICE 2
#define L_INFO   3
#define L_DBG    4

/* vars:*/

extern int debug;
extern int log_stderr;


#define DPRINT_LEV	1
/* log facility (see syslog(3)) */
#define L_FAC  LOG_DAEMON
/* priority at which we log */
#define DPRINT_PRIO LOG_DEBUG


void dprint (char* format, ...);

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
				if (debug>=DPRINT_LEV){ \
					if (log_stderr){ \
						dprint (__VA_ARGS__); \
					}else{ \
						syslog(DPRINT_LEV|L_FAC,  __VA_ARGS__); \
					}\
				} \
			}while(0)
	#else
			#define DPrint(fmt,args...) \
			do{ \
				if (debug>=DPRINT_LEV){ \
					if (log_stderr){ \
						dprint (fmt, ## args); \
					}else{ \
						syslog(DPRINT_LEV|L_FAC, fmt, ## args); \
					}\
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
				if (debug>=(lev)){ \
					if (log_stderr) dprint (__VA_ARGS__); \
					else { \
						switch(lev){ \
							case L_CRIT: \
								syslog(LOG_CRIT | L_FAC, __VA_ARGS__); \
								break; \
							case L_ALERT: \
								syslog(LOG_ALERT | L_FAC, __VA_ARGS__); \
								break; \
							case L_ERR: \
								syslog(LOG_ERR | L_FAC, __VA_ARGS__); \
								break; \
							case L_WARN: \
								syslog(LOG_WARNING | L_FAC, __VA_ARGS__);\
								break; \
							case L_NOTICE: \
								syslog(LOG_NOTICE | L_FAC, __VA_ARGS__); \
								break; \
							case L_INFO: \
								syslog(LOG_INFO | L_FAC, __VA_ARGS__); \
								break; \
							case L_DBG: \
								syslog(LOG_DEBUG | L_FAC, __VA_ARGS__); \
								break; \
						} \
					} \
				} \
			}while(0)
	#else
		#define LOG(lev, fmt, args...) \
			do { \
				if (debug>=(lev)){ \
					if (log_stderr) dprint (fmt, ## args); \
					else { \
						switch(lev){ \
							case L_CRIT: \
								syslog(LOG_CRIT | L_FAC, fmt, ##args); \
								break; \
							case L_ALERT: \
								syslog(LOG_ALERT | L_FAC, fmt, ##args); \
								break; \
							case L_ERR: \
								syslog(LOG_ERR | L_FAC, fmt, ##args); \
								break; \
							case L_WARN: \
								syslog(LOG_WARNING | L_FAC, fmt, ##args); \
								break; \
							case L_NOTICE: \
								syslog(LOG_NOTICE | L_FAC, fmt, ##args); \
								break; \
							case L_INFO: \
								syslog(LOG_INFO | L_FAC, fmt, ##args); \
								break; \
							case L_DBG: \
								syslog(LOG_DEBUG | L_FAC, fmt, ##args); \
								break; \
						} \
					} \
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

#endif /* ifndef dprint_h */
