/*
 * Copyright (C) 2007 iptelorg GmbH
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
/*!
* \file
* \brief Kamailio core :: local timer routines
* \ingroup core
* \author andrei
* Module: \ref core
*
 * WARNING: this should be used only from within the same process.
 * The local timers are not multi-process or multi-thread safe 
 *  (there are no locks)
 *
 */

#ifndef _local_timer_h
#define _local_timer_h

#include "timer_ticks.h"
#include "timer_funcs.h"


struct local_timer {
	/* private timer information */
	ticks_t prev_ticks; /* last time we ran the timer */
	struct timer_lists timer_lst; /* actual timer lists */
};


#define local_timer_init(tl, fun, param, flgs) timer_init(tl, fun, param, flgs)

#define local_timer_reinit(tl) timer_reinit((tl))

int init_local_timer(struct local_timer *lt_handle, ticks_t crt_ticks);
void destroy_local_timer(struct local_timer* lt_handle);

int local_timer_add(struct local_timer* h, struct timer_ln* tl, ticks_t delta,
						ticks_t crt_ticks);

void local_timer_del(struct local_timer* h, struct timer_ln* tl);
void local_timer_run(struct local_timer* lt, ticks_t crt_ticks);

#endif /* _local_timer_h */
