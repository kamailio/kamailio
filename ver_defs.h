/*
 * version and compile flags macros 
 *
 * Copyright (C) 2004 FhG Fokus
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

/** compile flags and other version related defines.
 * @file ver_defs.h
 * @ingroup core
 */

#ifndef version_h
#define version_h

#define SER_FULL_VERSION  NAME " " VERSION " (" ARCH "/" OS_QUOTED ")" 

#ifdef STATS
#define STATS_STR  "STATS: On"
#else
#define STATS_STR  "STATS: Off"
#endif

#ifdef USE_TCP
#define USE_TCP_STR ", USE_TCP"
#else
#define USE_TCP_STR ""
#endif

#ifdef USE_TLS
#define USE_TLS_STR ", USE_TLS"
#else 
#define USE_TLS_STR ""
#endif

#ifdef USE_SCTP
#define USE_SCTP_STR ", USE_SCTP"
#else
#define USE_SCTP_STR ""
#endif

#ifdef CORE_TLS
#define CORE_TLS_STR ", CORE_TLS"
#else 
#define CORE_TLS_STR ""
#endif

#ifdef TLS_HOOKS
#define TLS_HOOKS_STR ", TLS_HOOKS"
#else 
#define TLS_HOOKS_STR ""
#endif


#ifdef USE_RAW_SOCKS
#define USE_RAW_SOCKS_STR ", USE_RAW_SOCKS"
#else
#define USE_RAW_SOCKS_STR ""
#endif


#ifdef DISABLE_NAGLE
#define DISABLE_NAGLE_STR ", DISABLE_NAGLE"
#else
#define DISABLE_NAGLE_STR ""
#endif

#ifdef USE_MCAST
#define USE_MCAST_STR ", USE_MCAST"
#else
#define USE_MCAST_STR ""
#endif


#ifdef NO_DEBUG
#define NO_DEBUG_STR ", NO_DEBUG"
#else
#define NO_DEBUG_STR ""
#endif

#ifdef NO_LOG
#define NO_LOG_STR ", NO_LOG"
#else
#define NO_LOG_STR ""
#endif

#ifdef EXTRA_DEBUG
#define EXTRA_DEBUG_STR ", EXTRA_DEBUG"
#else
#define EXTRA_DEBUG_STR ""
#endif

#ifdef DNS_IP_HACK
#define DNS_IP_HACK_STR ", DNS_IP_HACK"
#else
#define DNS_IP_HACK_STR ""
#endif

#ifdef SHM_MEM
#define SHM_MEM_STR ", SHM_MEM"
#else
#define SHM_MEM_STR ""
#endif

#ifdef SHM_MMAP
#define SHM_MMAP_STR ", SHM_MMAP"
#else
#define SHM_MMAP_STR ""
#endif

#ifdef PKG_MALLOC
#define PKG_MALLOC_STR ", PKG_MALLOC"
#else
#define PKG_MALLOC_STR ""
#endif

#ifdef F_MALLOC
#define F_MALLOC_STR ", F_MALLOC"
#else
#define F_MALLOC_STR ""
#endif

#ifdef DL_MALLOC
#define DL_MALLOC_STR ", DL_MALLOC"
#else
#define DL_MALLOC_STR ""
#endif

#ifdef SF_MALLOC
#define SF_MALLOC_STR ", SF_MALLOC"
#else
#define SF_MALLOC_STR ""
#endif

#ifdef LL_MALLOC
#define LL_MALLOC_STR ", LL_MALLOC"
#else
#define LL_MALLOC_STR ""
#endif

#ifdef USE_SHM_MEM
#define USE_SHM_MEM_STR ", USE_SHM_MEM"
#else
#define USE_SHM_MEM_STR ""
#endif

#ifdef DBG_QM_MALLOC
#define DBG_QM_MALLOC_STR ", DBG_QM_MALLOC"
#else
#define DBG_QM_MALLOC_STR ""
#endif

#ifdef DBG_F_MALLOC
#define DBG_F_MALLOC_STR ", DBG_F_MALLOC"
#else
#define DBG_F_MALLOC_STR ""
#endif

#ifdef DEBUG_DMALLOC
#define DEBUG_DMALLOC_STR ", DEBUG_DMALLOC"
#else
#define DEBUG_DMALLOC_STR ""
#endif

#ifdef DBG_SF_MALLOC
#define DBG_SF_MALLOC_STR ", DBG_SF_MALLOC"
#else
#define DBG_SF_MALLOC_STR ""
#endif

#ifdef DBG_LL_MALLOC
#define DBG_LL_MALLOC_STR ", DBG_SF_MALLOC"
#else
#define DBG_LL_MALLOC_STR ""
#endif

#ifdef TIMER_DEBUG
#define TIMER_DEBUG_STR ", TIMER_DEBUG"
#else
#define TIMER_DEBUG_STR ""
#endif

#ifdef USE_FUTEX
#define USE_FUTEX_STR ", USE_FUTEX"
#else
#define USE_FUTEX_STR ""
#endif


#ifdef FAST_LOCK
#ifdef BUSY_WAIT
#define FAST_LOCK_STR ", FAST_LOCK-BUSY_WAIT"
#elif defined (ADAPTIVE_WAIT)
#define FAST_LOCK_STR ", FAST_LOCK-ADAPTIVE_WAIT"
#else
#define FAST_LOCK_STR ", FAST_LOCK"
#endif
#else
#define FAST_LOCK_STR ""
#endif

#ifdef USE_PTHREAD_MUTEX
#define USE_PTHREAD_MUTEX_STR ", USE_PTHREAD_MUTEX"
#else
#define USE_PTHREAD_MUTEX_STR ""
#endif

#ifdef USE_POSIX_SEM
#define USE_POSIX_SEM_STR ", USE_POSIX_SEM"
#else
#define USE_POSIX_SEM_STR ""
#endif

#ifdef USE_SYSV_SEM
#define USE_SYSV_SEM_STR ", USE_SYSV_SEM"
#else
#define USE_SYSV_SEM_STR ""
#endif

#ifdef NOSMP
#define NOSMP_STR "-NOSMP"
#else
#define NOSMP_STR ""
#endif


#ifdef USE_COMP
#define USE_COMP_STR ", USE_COMP"
#else
#define USE_COMP_STR ""
#endif


#ifdef USE_DNS_CACHE
#define USE_DNS_CACHE_STR ", USE_DNS_CACHE"
#else
#define USE_DNS_CACHE_STR ""
#endif

#ifdef USE_DNS_FAILOVER
#define USE_DNS_FAILOVER_STR ", USE_DNS_FAILOVER"
#else
#define USE_DNS_FAILOVER_STR ""
#endif

#ifdef DNS_WATCHDOG_SUPPORT
#define DNS_WATCHDOG_SUPPORT_STR ", DNS_WATCHDOG_SUPPORT"
#else
#define DNS_WATCHDOG_SUPPORT_STR ""
#endif

#ifdef USE_NAPTR
#define USE_NAPTR_STR ", USE_NAPTR"
#else
#define USE_NAPTR_STR ""
#endif

#ifdef USE_DST_BLACKLIST
#define USE_DST_BLACKLIST_STR ", USE_DST_BLACKLIST"
#else
#define USE_DST_BLACKLIST_STR ""
#endif

#ifdef NO_SIG_DEBUG
#define NO_SIG_DEBUG_STR ", NO_SIG_DEBUG"
#else
#define NO_SIG_DEBUG_STR ""
#endif

#ifdef HAVE_RESOLV_RES 
#define HAVE_RESOLV_RES_STR ", HAVE_RESOLV_RES"
#else
#define HAVE_RESOLV_RES_STR ""
#endif

#ifdef MEM_JOIN_FREE
#define MEM_JOIN_FREE_STR ", MEM_JOIN_FREE"
#else
#define MEM_JOIN_FREE_STR ""
#endif

#ifdef SYSLOG_CALLBACK_SUPPORT 
#define SYSLOG_CALLBACK_SUPPORT_STR, ", SYSLOG_CALLBACK_SUPPORT"
#else
#define SYSLOG_CALLBACK_SUPPORT_STR ""
#endif

#ifdef MYSQL_FAKE_NULL
#define MYSQL_FAKE_NULL_STR, ", MYSQL_FAKE_NULL"
#else
#define MYSQL_FAKE_NULL_STR ""
#endif

#ifdef USE_DNS_CACHE_STATS
#define USE_DNS_CACHE_STATS_STR ", USE_DNS_CACHE_STATS"
#else
#define USE_DNS_CACHE_STATS_STR ""
#endif

#ifdef USE_DST_BLACKLIST_STATS
#define USE_DST_BLACKLIST_STATS_STR ", USE_DST_BLACKLIST_STATS"
#else
#define USE_DST_BLACKLIST_STATS_STR ""
#endif

#define SER_COMPILE_FLAGS \
	STATS_STR EXTRA_DEBUG_STR USE_TCP_STR USE_TLS_STR \
	USE_SCTP_STR CORE_TLS_STR TLS_HOOKS_STR  USE_RAW_SOCKS_STR \
	DISABLE_NAGLE_STR USE_MCAST_STR NO_DEBUG_STR NO_LOG_STR \
	NO_SIG_DEBUG_STR DNS_IP_HACK_STR  SHM_MEM_STR SHM_MMAP_STR PKG_MALLOC_STR \
	F_MALLOC_STR DL_MALLOC_STR SF_MALLOC_STR  LL_MALLOC_STR \
	USE_SHM_MEM_STR \
	DBG_QM_MALLOC_STR \
	DBG_F_MALLOC_STR DEBUG_DMALLOC_STR DBG_SF_MALLOC_STR DBG_LL_MALLOC_STR \
	TIMER_DEBUG_STR \
	USE_FUTEX_STR \
	FAST_LOCK_STR NOSMP_STR USE_PTHREAD_MUTEX_STR USE_POSIX_SEM_STR \
	USE_SYSV_SEM_STR USE_COMP_STR USE_DNS_CACHE_STR USE_DNS_FAILOVER_STR \
	DNS_WATCHDOG_SUPPORT_STR USE_NAPTR_STR USE_DST_BLACKLIST_STR \
	HAVE_RESOLV_RES_STR SYSLOG_CALLBACK_SUPPORT_STR MYSQL_FAKE_NULL_STR \
	USE_DST_BLACKLIST_STATS_STR USE_DNS_CACHE_STATS_STR


#endif
