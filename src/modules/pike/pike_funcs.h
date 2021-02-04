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


#ifndef _PIKE_FUNCS_H
#define _PIKE_FUNCS_H

#include "../../core/parser/msg_parser.h"
#include "../../core/locking.h"


void pike_counter_init(void);
int  pike_check_req(sip_msg_t *msg);
int  pike_check_ip(sip_msg_t *msg, str *strip);
int  w_pike_check_req(sip_msg_t *msg, char *foo, char *bar);
int  w_pike_check_ip(sip_msg_t *msg, char *pip, char *bar);
void clean_routine(unsigned int, void*);
void swap_routine(unsigned int, void*);


#endif
