/*
 * $Id$
 *
 * Copyright (C) 2013 Crocodile RCS Ltd
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
 * Exception: permission to copy, modify, propagate, and distribute a work
 * formed by combining OpenSSL toolkit software and the code in this file,
 * such as linking with software components and libraries released under
 * OpenSSL project license.
 *
 */
#ifndef AUTHEPH_MOD_H
#define AUTHEPH_MOD_H

#include "../../locking.h"
#include "../../str.h"
#include "../../modules/auth/api.h"

struct secret
{
	str secret_key;
	struct secret *prev;
	struct secret *next;
};
extern struct secret *secret_list;

typedef enum {
	AUTHEPH_USERNAME_NON_IETF	= 0,
	AUTHEPH_USERNAME_IETF		= 1,
} autheph_username_format_t;
extern autheph_username_format_t autheph_username_format;

extern auth_api_s_t eph_auth_api;

extern gen_lock_t *autheph_secret_lock;
#define SECRET_LOCK	lock_get(autheph_secret_lock)
#define SECRET_UNLOCK	lock_release(autheph_secret_lock)

#endif /* AUTHEPH_MOD_H */
