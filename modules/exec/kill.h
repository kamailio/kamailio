/*
 * Copyright (C) 2001-2003 FhG Fokus
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
 * \brief Exec module:: Kill process
 * \ingroup exec 
 * Module: \ref exec
 */


#ifndef _KILL_H
#define _KILL_H

struct timer_link {
	struct timer_link *next_tl;
	struct timer_link *prev_tl;
	volatile unsigned int time_out;
	int pid;
};

struct timer_list
{
	struct timer_link  first_tl;
	struct timer_link  last_tl;
};

extern unsigned int time_to_kill;

void destroy_kill(void);
int initialize_kill(void);
int schedule_to_kill( int pid );


#endif

