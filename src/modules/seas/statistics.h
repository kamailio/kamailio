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

#include <sys/time.h>
#include <time.h>
#include "../../hashes.h"/* for TABLE_ENTRIES define*/
#include "../../locking.h"/* for TABLE_ENTRIES define*/
#include "../../modules/tm/h_table.h"/* for struct cell*/
#define STATS_CELLS 50
#define UAS_T 0
#define UAC_T 1
/**
 * stores statistics about a given APP SERVER,
 * for instance, how much it lasted to respond to
 * a given incoming request transaction, how many requests went in
 * and how many responses went out, etc.
 * this should be about... 16*4+20*4+4 bytes...64+80+4=148 bytes each cell
 */
struct statscell
{
   /** 0 = UAS, 1 = UAC*/
   char type;
   /**difference between a request_event and a reply_action*/
   union {
      struct {
	 struct timeval as_relay;
	 struct timeval event_sent;
	 struct timeval action_recvd;
      } uas;
      struct {
	 struct timeval action_recvd;
	 struct timeval event_sent;
	 struct timeval action_reply_sent;
      } uac;
   }u;
};

/** Transactions statistics table */
struct statstable
{
   gen_lock_t *mutex;
   unsigned int dispatch[15];
   unsigned int event[15];
   unsigned int action[15];
   unsigned int started_transactions;
   unsigned int finished_transactions;
   unsigned int received_replies;
   unsigned int received;
};

extern struct statstable *seas_stats_table;

/**
 * Initialize and destroy statistics table
 */
struct statstable* init_seas_stats_table(void);
int stop_stats_server(void);
void destroy_seas_stats_table(void);
/** Statistics server process
 * functions
 */
void serve_stats(int fd);
int start_stats_server(char *socket);
int print_stats_info(int f,int sock);
/**
 * Statistics functions
 */
void as_relay_stat(struct cell *t);
void event_stat(struct cell *t);
void action_stat(struct cell *t);
void stats_reply(void);
#define receivedplus() \
   do{ \
      lock_get(seas_stats_table->mutex); \
      seas_stats_table->received++; \
      lock_release(seas_stats_table->mutex); \
   }while(0)
