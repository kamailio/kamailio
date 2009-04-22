/*
 * $Id$
 *
 * Copyright (C) 2006 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * History:
 * ---------
 *  2006-01-16  first version (bogdan)
 *  2006-11-28  added get_stat_var_from_num_code() (Jeffrey Magder -
 *              SOMA Networks)
 */

/*!
 * \file
 * \brief Kamailio statistics handling
 */


#ifndef _KSTATISTICS_H_
#define _KSTATISTICS_H_

#include <string.h>
#include "../../str.h"
#include "../../atomic_ops.h"

#define STATS_HASH_POWER   8
#define STATS_HASH_SIZE    (1<<(STATS_HASH_POWER))

#define NUM_IP_OCTETS 4

#define STAT_NO_RESET  (1<<0)
#define STAT_NO_SYNC   (1<<1)
#define STAT_SHM_NAME  (1<<2)
#define STAT_IS_FUNC   (1<<3)

typedef atomic_t stat_val;

typedef unsigned long (*stat_function)(void);

struct module_stats_;

typedef struct stat_var_{
	unsigned int mod_idx;
	str name;
	int flags;
	union{
		stat_val *val;
		stat_function f;
	}u;
	struct stat_var_ *hnext;
	struct stat_var_ *lnext;
} stat_var;

typedef struct module_stats_ {
	str name;
	int no;
	stat_var *head;
	stat_var *tail;
} module_stats;

typedef struct stats_collector_ {
	int stats_no;
	int mod_no;
	stat_var* hstats[STATS_HASH_SIZE];
	module_stats *amodules;
}stats_collector;

typedef struct stat_export_ {
	char* name;                /* null terminated statistic name */
	int flags;                 /* flags */
	stat_var** stat_pointer;   /* pointer to the variable's mem location *
	                            * NOTE - it's in shm mem */
} stat_export_t;


#ifdef STATISTICS
int init_stats_collector(void);

void destroy_stats_collector(void);

int register_stat( char *module, char *name, stat_var **pvar, int flags);

int register_module_stats(char *module, stat_export_t *stats);

stat_var* get_stat( str *name );

unsigned int get_stat_val( stat_var *var );

/*! \brief
 * Returns the statistic associated with 'numerical_code' and 'is_a_reply'.
 * Specifically:
 *
 *  - if in_codes is nonzero, then the stat_var for the number of messages 
 *    _received_ with the 'numerical_code' will be returned if it exists.
 *  - otherwise, the stat_var for the number of messages _sent_ with the 
 *    'numerical_code' will be returned, if the stat exists. 
 */
stat_var *get_stat_var_from_num_code(unsigned int numerical_code, int in_codes);

stats_collector* get_stats_collector(void);
module_stats* get_stat_module(str *module);

#else
	#define init_stats_collector()  0
	#define destroy_stats_collector()
	#define register_module_stats(_mod,_stats) 0
	#define register_stat( _mod, _name, _pvar, _flags) 0
	#define get_stat( _name )  0
	#define get_stat_val( _var ) 0
	#define get_stat_var_from_num_code( _n_code, _in_code) NULL
	#define get_stats_collector() NULL
	#define get_stat_module(_module) NULL
#endif


#ifdef STATISTICS
	#define update_stat( _var, _n) \
		do { \
			if ( !((_var)->flags&STAT_IS_FUNC) ) {\
				atomic_add((_var)->u.val, _n);\
			}\
		}while(0)
	#define reset_stat( _var) \
		do { \
			if ( ((_var)->flags&(STAT_NO_RESET|STAT_IS_FUNC))==0 ) {\
				atomic_set( (_var)->u.val, 0);\
			}\
		}while(0)
	#define get_stat_val( _var ) ((unsigned long)\
		((_var)->flags&STAT_IS_FUNC)?(_var)->u.f():(_var)->u.val->val)

	#define if_update_stat(_c, _var, _n) \
		do { \
			if (_c) update_stat( _var, _n); \
		}while(0)
	#define if_reset_stat(_c, _var) \
		do { \
			if (_c) reset_stat( _var); \
		}while(0)
#else
	#define update_stat( _var, _n)
	#define reset_stat( _var)
	#define if_update_stat( _c, _var, _n)
	#define if_reset_stat( _c, _var)
#endif /*STATISTICS*/

/*!
 * This function will retrieve a list of all ip addresses and ports that OpenSER
 * is listening on, with respect to the transport protocol specified with
 * 'protocol'. 
 *
 * The first parameter, ipList, is a pointer to a pointer. It will be assigned a
 * new block of memory holding the IP Addresses and ports being listened to with
 * respect to 'protocol'.  The array maps a 2D array into a 1 dimensional space,
 * and is layed out as follows:
 *
 * The first NUM_IP_OCTETS indices will be the IP address, and the next index
 * the port.  So if NUM_IP_OCTETS is equal to 4 and there are two IP addresses
 * found, then:
 *
 *  - ipList[0] will be the first octet of the first ip address
 *  - ipList[3] will be the last octet of the first ip address.
 *  - iplist[4] will be the port of the first ip address
 *  - 
 *  - iplist[5] will be the first octet of the first ip address, 
 *  - and so on.  
 *
 * The function will return the number of sockets which were found.  This can be
 * used to index into ipList.
 *
 * \note This function assigns a block of memory equal to:
 *
 *            returnedValue * (NUM_IP_OCTETS + 1) * sizeof(int);
 *
 *       Therefore it is CRUCIAL that you free ipList when you are done with its
 *       contents, to avoid a nasty memory leak.
 */
int get_socket_list_from_proto(int **ipList, int protocol);


/*!
 * Returns the sum of the number of bytes waiting to be consumed on all network
 * interfaces and transports that OpenSER is listening on. 
 *
 * Note: This currently only works on systems supporting the /proc/net/[tcp|udp]
 *       interface.  On other systems, zero will always be returned.  Details of
 *       why this is so can be found in network_stats.c
 */
int get_total_bytes_waiting(void);


#endif
