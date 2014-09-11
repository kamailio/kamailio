/*
 * extra.h
 *
 * Copyright (C) 2008 Juha Heinanen <jh@tutpro.com>
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

#ifndef _MISC_RADIUS_EXTRA_H_
#define _MISC_RADIUS_EXTRA_H_

#include "../../str.h"
#include "../../pvar.h"
#include "../../parser/msg_parser.h"
#include "../../lib/kcore/radius.h"

struct extra_attr {
    str name;
    pv_spec_t spec;
    struct extra_attr *next;
};

#define MAX_EXTRA 4

void init_extra_engine(void);

struct extra_attr *parse_extra_str(char *extra);

void destroy_extras(struct extra_attr *extra);

int extra2strar(struct extra_attr *extra, struct sip_msg *rq, str *val_arr);

int extra2attrs(struct extra_attr *extra, struct attr *attrs, int offset);

#endif
