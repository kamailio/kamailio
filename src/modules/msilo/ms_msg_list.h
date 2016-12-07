/**
 * MSILO module
 *
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
 *
 */

#ifndef _MS_MSG_LIST_H_
#define _MS_MSG_LIST_H_


#include "../../locking.h"

#define MS_MSG_NULL	0
#define MS_MSG_SENT	1
#define MS_MSG_DONE	4
#define MS_MSG_ERRO	8
#define MS_MSG_TSND	16

#define MS_SEM_SENT	0
#define MS_SEM_DONE 1

#define MSG_LIST_OK		0
#define MSG_LIST_ERR	-1
#define MSG_LIST_EXIST	1

typedef struct _msg_list_el
{
	int msgid;
	int flag;
	struct _msg_list_el * prev;
	struct _msg_list_el * next;
} t_msg_list_el, *msg_list_el;

typedef struct _msg_list
{
	int nrsent;
	int nrdone;
	msg_list_el lsent;
	msg_list_el ldone;
	gen_lock_t  sem_sent;
	gen_lock_t  sem_done;
} t_msg_list, *msg_list;

msg_list_el msg_list_el_new(void);
void msg_list_el_free(msg_list_el);
void msg_list_el_free_all(msg_list_el);

msg_list msg_list_init(void);
void msg_list_free(msg_list);
int msg_list_check_msg(msg_list, int);
int msg_list_set_flag(msg_list, int, int);
int msg_list_check(msg_list);
msg_list_el msg_list_reset(msg_list);

#endif

