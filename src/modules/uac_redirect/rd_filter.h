/*
 * Copyright (C) 2005 Voice Sistem SRL
 *
 * This file is part of Kamailio, a free SIP server.
 *
 * Kamailio is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * Kamailio is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */


#ifndef _REDIRECT_FILTER_H
#define _REDIRECT_FILTER_H

#include <sys/types.h> /* for regex */
#include <regex.h>

#define ACCEPT_FILTER   0
#define DENY_FILTER     1
#define NR_FILTER_TYPES 2

#define ACCEPT_RULE    11
#define DENY_RULE      12

#define RESET_ADDED    (1<<0)
#define RESET_DEFAULT  (1<<1)

void init_filters(void);
void set_default_rule( int type );
void reset_filters(void);
void add_default_filter( int type, regex_t *filter);
int add_filter( int type, regex_t *filter, int flags);
int run_filters(char *s);

#endif
