/*
 * $Id$
 *
 * imc module - instant messaging conferencing implementation
 *
 * Copyright (C) 2006 Voice Sistem S.R.L.
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
 * History:
 * ---------
 *  2006-10-06  first version (anca)
 */



#ifndef _IMC_MNG_H_
#define _IMC_MNG_H_



#include "../../locking.h"
#include "../../str.h"
#include "../../parser/parse_from.h"

#define IMC_MEMBER_OWNER	(1<<0)
#define IMC_MEMBER_ADMIN	(1<<1)
#define IMC_MEMBER_INVITED	(1<<2)
#define IMC_MEMBER_DELETED  (1<<3)
#define IMC_MEMBER_SKIP     (1<<4)

typedef struct _imc_member
{
	unsigned int hashid;
	str uri;
	str user;
	str domain;
	int flags;
	struct _imc_member * next;
	struct _imc_member * prev;
} imc_member_t, *imc_member_p;

#define IMC_ROOM_PRIV		(1<<0)
#define IMC_ROOM_DELETED	(1<<1)
typedef struct del_member
{
	str room_name;
	str room_domain;
	str inv_uri;
	str member_name;
	str member_domain;
}del_member_t;


typedef struct _imc_room
{
	unsigned int hashid;
	str uri;
	str name;
	str domain;
	int flags;
	int nr_of_members;
	imc_member_p members;
	struct _imc_room * next;
	struct _imc_room * prev;
} imc_room_t, *imc_room_p;

typedef struct _imc_hentry
{
	imc_room_p rooms;
	gen_lock_t lock;
} imc_hentry_t, *imc_hentry_p;

imc_member_p imc_add_member(imc_room_p room, str* user, str* domain, int flags);
imc_member_p imc_get_member(imc_room_p room, str* user, str* domain);
int imc_del_member(imc_room_p room, str* user, str* domain);

imc_room_p imc_add_room(str* name, str* domain, int flags);
imc_room_p imc_get_room(str* name, str* domain);
int imc_del_room(str* name, str* domain);
int imc_release_room(imc_room_p room);

int imc_htable_init(void);
int imc_htable_destroy(void);



#endif

