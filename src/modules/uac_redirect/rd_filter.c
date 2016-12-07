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
 *
 */


#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>

#include "../../dprint.h"
#include "rd_filter.h"

#define MAX_FILTERS 6

static int default_rule = ACCEPT_RULE;

static regex_t *rd_filters[NR_FILTER_TYPES][MAX_FILTERS];
static int nr_filters[NR_FILTER_TYPES];
static int start_filters[NR_FILTER_TYPES];


void init_filters(void)
{
	memset( rd_filters , 0, NR_FILTER_TYPES*MAX_FILTERS*sizeof(regex_t*));
	reset_filters();
}


void set_default_rule( int type )
{
	default_rule = type;
}


void reset_filters(void)
{
	nr_filters[ACCEPT_FILTER] = 1;
	nr_filters[DENY_FILTER]   = 1;
	start_filters[ACCEPT_FILTER] = 0;
	start_filters[DENY_FILTER]   = 0;
}


void add_default_filter( int type, regex_t *filter)
{
	rd_filters[type][0] = filter;
}


int add_filter( int type, regex_t *filter, int flags)
{
	if ( nr_filters[type]==MAX_FILTERS ) {
		LM_ERR("too many filters type %d\n", type);
		return -1;
	}

	/* flags? */
	if (flags&RESET_ADDED)
		nr_filters[type] = 1;
	if (flags&RESET_DEFAULT)
		start_filters[type] = 1;

	/* set filter */
	rd_filters[type][ nr_filters[type]++ ] = filter;
	return 0;
}


int run_filters(char *s)
{
	regmatch_t pmatch;
	int i;

	/* check for accept filters */
	for( i=start_filters[ACCEPT_FILTER] ; i<nr_filters[ACCEPT_FILTER] ; i++ ) {
		if (rd_filters[ACCEPT_FILTER][i]==0)
			continue;
		if (regexec(rd_filters[ACCEPT_FILTER][i], s, 1, &pmatch, 0)==0)
			return 1;
	}

	/* if default rule is deny, don' check the deny rules */
	if (default_rule!=DENY_RULE) {
		/* check for deny filters */
		for( i=start_filters[DENY_FILTER] ; i<nr_filters[DENY_FILTER] ; i++ ) {
			if (rd_filters[DENY_FILTER][i]==0)
				continue;
			if (regexec(rd_filters[DENY_FILTER][i], s, 1, &pmatch, 0)==0)
				return -1;
		}
	}

	/* return default */
	return (default_rule==ACCEPT_RULE)?1:-1;
}

