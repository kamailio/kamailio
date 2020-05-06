/**
 * keepalive module - remote destinations probing
 *
 * Copyright (C) 2017 Guillaume Bour <guillaume@bour.cc>
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

/*! \file
 * \ingroup keepalive
 * \brief Keepalive :: Keepalive
 */

#ifndef _KEEPALIVE_H_
#define _KEEPALIVE_H_

#include <time.h>
#include "../../core/sr_module.h"
#include "../../core/locking.h"

#define KA_INACTIVE_DST 1 /*!< inactive destination */
#define KA_TRYING_DST 2   /*!< temporary trying destination */
#define KA_DISABLED_DST 4 /*!< admin disabled destination */
#define KA_PROBING_DST 8  /*!< checking destination */
#define KA_STATES_ALL 15  /*!< all bits for the states of destination */

extern int ka_ping_interval;

#define ds_skip_dst(flags) ((flags) & (KA_INACTIVE_DST | KA_DISABLED_DST))

#define KA_PROBE_NONE 0
#define KA_PROBE_ALL 1
#define KA_PROBE_INACTIVE 2
#define KA_PROBE_ONLYFLAGGED 3

typedef void (*ka_statechanged_f)(str *uri, int state, void *user_attr);


typedef struct _ka_dest
{
	str uri;
	str owner; // name of destination "owner"
			   // (module asking to monitor this destination
	int flags;
	int state;
	time_t last_checked, last_up, last_down;
	int counter;	// counts unreachable attemps

	void *user_attr;
	ka_statechanged_f statechanged_clb;
	struct socket_info *sock;
	struct ip_addr ip_address; /*!< IP-Address of the entry */
	unsigned short int port;   /*!< Port of the URI */
	unsigned short int proto;  /*!< Protocol of the URI */
	struct timer_ln *timer;
	struct _ka_dest *next;
} ka_dest_t;

typedef struct _ka_destinations_list
{
	gen_lock_t *lock;
	ka_dest_t *first;
} ka_destinations_list_t;

extern ka_destinations_list_t *ka_destinations_list;
extern int ka_counter_del;

ticks_t ka_check_timer(ticks_t ticks, struct timer_ln* tl, void* param);

int ka_add_dest(str *uri, str *owner, int flags, int ping_interval,
        ka_statechanged_f callback, void *user_attr);
int ka_destination_state(str *uri);
int ka_str_copy(str *src, str *dest, char *prefix);
int free_destination(ka_dest_t *dest) ;
int ka_del_destination(str *uri, str *owner) ;
int ka_find_destination(str *uri, str *owner, ka_dest_t **target ,ka_dest_t **head);
int ka_lock_destination_list();
int ka_unlock_destination_list();
#endif
