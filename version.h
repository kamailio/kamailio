/*
 * $Id$
 *
 * version and compile flags macros 
 *
 *
 * Copyright (C) 2004 FhG Fokus
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

#ifndef version_h
#define version_h

#define SER_FULL_VERSION  NAME " " VERSION " (" ARCH "/" OS ")" 


#ifdef STATS
#define STATS_STR  "STATS: On"
#else
#define STATS_STR  "STATS: Off"
#endif

#ifdef USE_IPV6
#define USE_IPV6_STR ", USE_IPV6"
#else
#define USE_IPV6_STR ""
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

#ifdef VQ_MALLOC
#define VQ_MALLOC_STR ", VQ_MALLOC"
#else
#define VQ_MALLOC_STR ""
#endif

#ifdef F_MALLOC
#define F_MALLOC_STR ", F_MALLOC"
#else
#define F_MALLOC_STR ""
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


#define SER_COMPILE_FLAGS \
	EXTRA_DEBUG_STR STATS_STR USE_IPV6_STR USE_TCP_STR USE_TLS_STR \
	DISABLE_NAGLE_STR USE_MCAST_STR NO_DEBUG_STR NO_LOG_STR DNS_IP_HACK_STR \
	SHM_MEM_STR SHM_MMAP_STR PKG_MALLOC_STR VQ_MALLOC_STR F_MALLOC_STR \
	USE_SHM_MEM_STR DBG_QM_MALLOC_STR DBG_F_MALLOC_STR DEBUG_DMALLOC_STR \
	FAST_LOCK_STR NOSMP_STR USE_PTHREAD_MUTEX_STR USE_POSIX_SEM_STR \
	USE_SYSV_SEM_STR


#endif
