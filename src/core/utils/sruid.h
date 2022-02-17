/*
 * sruid - unique id generator
 *
 * Copyright (C) 2012 Daniel-Constantin Mierla (asipto.com)
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
* \brief core/utils :: Unique ID generator
* \ingroup core/utils
* Module: \ref core/utils
*/

#ifndef _SRUID_H_
#define _SRUID_H_

#include "../../core/str.h"

#define SRUID_SIZE	64

typedef enum {SRUID_INC=0, SRUID_LFSR=1} sruid_mode_t;

typedef struct sruid {
	char buf[SRUID_SIZE];
	char *out;
	str uid;
	unsigned int counter;
	int pid;
	sruid_mode_t mode;
} sruid_t;

int sruid_init(sruid_t *sid, char sep, char *cid, int mode);
int sruid_next(sruid_t *sid);
int sruid_next_safe(sruid_t *sid);

int sruid_nextx(sruid_t *sid, str *x);
int sruid_nextx_safe(sruid_t *sid, str *x);

int sruid_nexthid(sruid_t *sid, str *sval);
int sruid_nexthid_safe(sruid_t *sid, str *sval);

typedef int (*sruid_uuid_generate_f)(char *out, int *len);

typedef struct sruid_uuid_api {
	sruid_uuid_generate_f fgenerate;
	sruid_uuid_generate_f fgenerate_time;
	sruid_uuid_generate_f fgenerate_random;
} sruid_uuid_api_t;

int sruid_uuid_api_set(sruid_uuid_api_t *sapi);
int sruid_uuid_generate(char *out, int *len);
int sruid_uuid_generate_time(char *out, int *len);
int sruid_uuid_generate_random(char *out, int *len);

#endif
