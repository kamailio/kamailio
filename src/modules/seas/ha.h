/* $Id$
 *
 * Copyright (C) 2006-2007 VozTelecom Sistemas S.L
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


#ifndef HA_H
#define HA_H
#include "../../locking.h"/* for get_lock_t define*/
#include <time.h>
struct ping{
   unsigned int id;
   struct timeval sent;
   struct ping *next;
};

struct ha{
   int timed_out_pings;
   int timeout;
   gen_lock_t *mutex;
   struct ping *pings; 
   int begin;
   int end;
   int count;
   int size;
};

extern char *jain_ping_config;
extern int jain_ping_period;
extern int jain_ping_timeout;
extern struct ping *jain_pings;

extern pid_t pinger_pid;

extern char *servlet_ping_config;
extern int servlet_ping_period;
extern int servlet_ping_timeout;
extern struct ping *servlet_pings;

extern int use_ha;

char * create_ping_event(int *evt_len,int flags,unsigned int *seqno);
int prepare_ha(void);
int spawn_pinger(void);
int print_pingtable(struct ha *ta,int idx,int lock);
int init_pingtable(struct ha *table,int timeout,int maxpings);
void destroy_pingtable(struct ha *table);
#endif
